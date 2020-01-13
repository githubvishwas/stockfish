(function ()
{
    "use strict";
    
    var board_el = G.cde("div");
    var board = BOARD(board_el);
    var zobrist_keys;
    var stalemate_by_rules;
    var evaler;
    var loading_el;
    var player1_el = G.cde("div", {c: "player player_white player_left"});
    var player2_el = G.cde("div", {c: "player player_black player_right"});
    var center_el  = G.cde("div", {c: "center_el"});
    var rating_slider;
    var new_game_el;
    var setup_game_el;
    var game_info_text;
    var starting_new_game;
    var retry_move_timer;
    var clock_manager;
    var pieces_moved;
    var startpos;
    var debugging = true;
    var legal_move_engine;
    var cur_pos_cmd;
    var game_history;
    var eval_depth = 12;
    var rating_font_style = "Impact,monospace,mono,sans-serif";
    var font_fit = FONT_FIT({fontFamily: rating_font_style});
    var moves_manager;
    var layout = {};
    var default_sd_time = "15:00";
    var showing_loading;
    var gameType = "standard";
    var lastGameType;
    var answers;
    var currentMovePath;
    var latest_best_move;
    var latst_eval_score;
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
            console.log("onmessage: " + line);
            
            /// Stream everything to this, even invalid lines.
            if (engine.stream) {
                engine.stream(line);
            }
            
            /// Ignore invalid setoption commands since valid ones do not repond.
            if (line.substr(0, 14) === "No such option") {
                return;
            }
            
            que_num = determine_que_num(line, que);
            
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
			} else if (line.substr(0, 8) === "depth 10") {
				console.log("score eval==");
                console.log("==" + line);
				console.log(line.split(/\b\s+/))
            } else if (line.substr(0, 8) === "bestmove") {
                /// go [...]
                done = true;
                console.log("bestmove==");
                console.log("==" + line);
                bestmove = line.substr(9,4)
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
                    my_que.cb(my_que.message);
                }
                
                /// Remove this from the que.
                G.array_remove(que, que_num);
                
                statusEl.html("Best move is: " + bestmove);
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
        
        alert("new init2")
        evaler = load_engine();
        alert("loaded2")
        evaler.send("uci", function onuci(str)
        {
			alert("uci2")
            evaler.send("isready", function onready()
            {
                alert("isready2")
                evaler.send("position fen 1rb1kbr1/pppp1p1p/4p1p1/7q/6Q1/8/PPPPP2P/RNBK3R w - -");
                evaler.send("go depth 10");
            });
        });
    }
    
    
    
    
    G.events.attach("move", function onmove(e)
    {
        var ply = game_history.length,
            color;
        
        if (!pieces_moved) {
            G.events.trigger("firstMove");
            pieces_moved = true;
        }
        
        /// player.last_move_time
        ///NOTE: board.turn has already switched.
        color = board.turn === "b" ? "w" : "b";
        game_history[ply] = {move: e.uci, ponder: e.ponder, turn: board.turn, pos: cur_pos_cmd, color: color};
        
        if (board.players[color].has_time) {
            game_history[ply].move_time = board.players[color].last_move_time;
        }
        prep_eval(cur_pos_cmd, ply);
        moves_manager.add_move({color: color, san: e.san, time: game_history[ply].move_time, ply: ply - 1, scoll_to_bottom: true});
    });
    
    G.events.attach("newGameBegins", function onmove(e)
    {
        moves_manager.reset_moves();
    });
    
    init();
}());
