(function ()
{
    "use strict";

    var evaler;
    
	var current_comp_score = 0.0
	var best_computer_move = ""
    var debugging = false;

    var statusEl = $('#status');
    statusEl.html("Calculation Started...");	
    function error(str)
    {
        str = str || "Unknown error";
        
        alert("An error occured.\n" + str);
        throw new Error(str);
    }
    
    function load_engine()
    {
        var worker = new Worker("js/stockfish6.js"),
            engine = {started: Date.now()},
            que = [];
        
        function get_first_word(line)
        {
			console.log("get_first_word")
            var space_index = line.indexOf(" ");
            
            /// If there are no spaces, send the whole line.
            if (space_index === -1) {
                return line;
            }
            return line.substr(0, space_index);
        }
        
        function determine_que_num(line, que)
        {
			console.log("determine_que_num")
            var cmd_type,
                first_word = get_first_word(line),
                cmd_first_word,
                i,
                len;
            
            if (first_word === "uciok" || first_word === "option") {
                cmd_type = "uci"
            } else if (first_word === "readyok") {
                cmd_type = "isready";
            } else if (first_word === "bestmove" || first_word === "info") {
                cmd_type = "go";
            } else {
                /// eval and d are more difficult.
                cmd_type = "other";
            }
            
            len = que.length;
            
            for (i = 0; i < len; i += 1) {
                cmd_first_word = get_first_word(que[i].cmd);
                if (cmd_first_word === cmd_type || (cmd_type === "other" && (cmd_first_word === "d" || cmd_first_word === "eval"))) {
                    return i;
                }
            }
            
            /// Not sure; just go with the first one.
            return 0;
        }
        
        worker.onmessage = function (e)
        {
            var line = e.data,
                done,
                que_num = 0,
                bestmove = "",
                my_que;
			if(debugging) {
				console.log("onmessage: " + line);
			}
            

            /// Stream everything to this, even invalid lines.
            if (engine.stream) {
                engine.stream(line);
            }
            
            /// Ignore invalid setoption commands since valid ones do not repond.
            if (line.substr(0, 14) === "No such option") {
                return;
            }
            
            que_num = determine_que_num(line, que);
            console.log("*** Queue num: " + que_num)
			
            my_que = que[que_num];
            
            if (!my_que) {
                return;
            }
			
            if (my_que.stream) {
                my_que.stream(line);
            }
            
            if (typeof my_que.message === "undefined") {
                my_que.message = "";
            } else if (my_que.message !== "") {
                my_que.message += "\n";
            }
            
            my_que.message += line;
            	
            /// Try to determine if the stream is done.
            if (line === "uciok") {
                /// uci
                done = true;
                engine.loaded = true;
            } else if (line === "readyok") {
                /// isready
                done = true;
                engine.ready = true;
			} else if (line.substr(0, 13) === "info depth 20") {
				console.log("*** Computer eval score: " + line.split(/\b\s+/)[9]/100.0);
				current_comp_score = line.split(/\b\s+/)[9]/100.0
            } else if (line.substr(0, 8) === "bestmove") {
                /// go [...]
                done = true;
                console.log("bestmove==");
                console.log("==" + line);
                bestmove = line.substr(9,4)
				best_computer_move = bestmove
                /// All "go" needs is the last line (use stream to get more)
                my_que.message = line;
            } else if (my_que.cmd === "d" && line.substr(0, 15) === "Legal uci moves") {
                done = true;
            } else if (my_que.cmd === "eval" && /Total Evaluation[\s\S]+\n$/.test(my_que.message)) {
                done = true;
                console.log("eval");
                console.log(line);
            } else if (line.substr(0, 15) === "Unknown command") {
                done = true;
            }
            ///NOTE: Stockfish.js does not support the "debug" or "register" commands.
            ///TODO: Add support for "perft", "bench", and "key" commands.
            ///TODO: Get welcome message so that it does not get caught with other messages.
            ///TODO: Prevent (or handle) multiple messages from different commands
            ///      E.g., "go depth 20" followed later by "uci"
            
            if (done) {
                if (my_que.cb && !my_que.discard) {
					console.log("*** my_que.cb: " + my_que.cb)
					console.log("*** my_que.message: " + my_que.message)
                    my_que.cb(my_que.message);
                }
                
                /// Remove this from the que.
                G.array_remove(que, que_num);
                if (line.substr(0, 8) === "bestmove") {
					statusEl.html("Ver 11 Best move is: " + best_computer_move + "\n Computer score: " + current_comp_score);
				} else {
					statusEl.html("Calculating, please wait...");
				}
                //cal_done()
            }
        };
        
        engine.send = function send(cmd, cb, stream)
        {
			console.log("engine send")
            cmd = String(cmd).trim();
            
            /// Can't quit. This is a browser.
            ///TODO: Destroy the engine.
            if (cmd === "quit") {
                return;
            }
            
            if (debugging) {
                console.log("1: " + cmd);
            }
            
            /// Only add a que for commands that always print.
            ///NOTE: setoption may or may not print a statement.
            if (cmd !== "ucinewgame" && cmd !== "flip" && cmd !== "stop" && cmd !== "ponderhit" && cmd.substr(0, 8) !== "position"  && cmd.substr(0, 9) !== "setoption") {
				console.log("cmd: " + cmd)
				console.log("cb: " + cb)
				console.log("stram: " + stream)
                que[que.length] = {
                    cmd: cmd,
                    cb: cb,
                    stream: stream
                };
            }
            worker.postMessage(cmd);
        };
        
        engine.stop_moves = function stop_moves()
        {
			console.log("stop moves")
            var i,
                len = que.length;
            
            for (i = 0; i < len; i += 1) {
                if (debugging) {
                    console.log(i, get_first_word(que[i].cmd))
                }
                /// We found a move that has not been stopped yet.
                if (get_first_word(que[i].cmd) === "go" && !que[i].discard) {
                    engine.send("stop");
                    que[i].discard = true;
                }
            }
        }
        
        engine.get_cue_len = function get_cue_len()
        {
			console.log("get_cue_len")
            return que.length;
        }
        
        return engine;
    }
    
    
    
   
    function init()
    {
        evaler = load_engine();
        
        evaler.send("uci", function onuci(str)
        {
			console.log("**** str: " +str)
            evaler.send("isready", function onready()
            {
                
                evaler.send("position fen 1rb1kbr1/pppp1p1p/4p1p1/7q/6Q1/8/PPPPP2P/RNBK3R w - -");
                evaler.send("go depth 20");
            });
        });
    }

    
    init();
}());
