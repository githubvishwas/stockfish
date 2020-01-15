function engineGame(options) {
    options = options || {}
    var game = new Chess();
    var comp_eval_score = []
	var curr_move_score = 0.0
	var eval_depth = '10'
    /// We can load Stockfish via Web Workers or via STOCKFISH() if loaded from a <script> tag.
    var engine = typeof STOCKFISH === "function" ? STOCKFISH() : new Worker(options.stockfishjs || 'stockfish.js');
    var evaler = typeof STOCKFISH === "function" ? STOCKFISH() : new Worker(options.stockfishjs || 'stockfish.js');
    
    evaler.onmessage = function(event) {
        var line;
        
        if (event && typeof event === "object") {
            line = event.data;
        } else {
            line = event;
        }
        
        //console.log("evaler: " + line);
        
       
    }

    engine.onmessage = function(event) {
        var line;
        
        if (event && typeof event === "object") {
            line = event.data;
        } else {
            line = event;
        }
        //console.log("Reply: " + line)
        if(line === undefined) {
			console.log("done!")
			comp_eval_score.push(curr_move_score)
			analyzePrevMove();
		}
        var match = line.match(/^bestmove ([a-h][1-8])([a-h][1-8])([qrbn])?/);
           
		if(match) {
		   console.log("done!")
		   comp_eval_score.push(curr_move_score)
		   analyzePrevMove();
		} else if(match = line.match(/^info .*\bscore (\w+) (-?\d+)/)) {
			var score = parseInt(match[2]) * (game.turn() == 'w' ? 1 : -1);
			/// Is it measuring in centipawns?
			if(match[1] == 'cp') {
				curr_move_score = (score / 100.0).toFixed(2);
			/// Did it find a mate?
			} else if(match[1] == 'mate') {
				curr_move_score = 'Mate in ' + Math.abs(score);
			}
			
			/// Is the score bounded?
			if(match = line.match(/\b(upper|lower)bound\b/)) {
				curr_move_score = ((match[1] == 'upper') == (game.turn() == 'w') ? '<= ' : '>= ') + engineStatus.score
			}
		}
            
            
    };
	function analyzePrevMove() {	
		var ret = game.undo()
		if (ret != null) {
			console.log("Analyzing fen: " + game.fen())
			uciCmd('position fen '+game.fen());
			uciCmd('position fen '+game.fen(), evaler);
			uciCmd('eval');
			uciCmd('go depth ' + eval_depth);
		} else {
			console.log("Game analysis over.")
			comp_eval_score1 = comp_eval_score.reverse()
			console.log(comp_eval_score1)
			
			var pgnwithscore = ""
			var newMove = 1
			var mvCounter = 1
			for (i = 0; i < pgnmoves.length; i++) {
				if (newMove === 1) {
					pgnwithscore = pgnwithscore + mvCounter + ". " + pgnmoves[i] + " {" + comp_eval_score1[i] + "} "
					newMove = 0
				} else {
					pgnwithscore = pgnwithscore + pgnmoves[i] + " {" + comp_eval_score1[i] + "} "
					newMove = 1
					mvCounter = mvCounter + 1
				}
			}
			var end = new Date().getTime();
			console.log(end - start);
			console.log(pgnwithscore)
			document.getElementById('pgn_with_comp_eval').value = pgnwithscore
		}
		
	}
    function uciCmd(cmd, which) {
        //console.log("UCI: " + cmd);
        
        (which || engine).postMessage(cmd);
    }
	
	var pgnfiletext = document.getElementById('ms_word_filtered_html').value	
	var games = pgnfiletext.split("[Event")
	var game1 = "[Event" +games[1] 
	game.load_pgn(game1)
	var start = new Date().getTime()
	var pgnmoves = game.history()
	console.log(pgnmoves)
	
	uciCmd('uci');
	uciCmd('ucinewgame');
    uciCmd('isready');
	console.log("Analyzing fen: " + game.fen())
	uciCmd('position fen '+game.fen());
    uciCmd('position fen '+game.fen(), evaler);
	uciCmd('eval');
	uciCmd('go depth '+ eval_depth);
}
