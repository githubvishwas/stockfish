function engineGame(options) {
    options = options || {}
    var game = new Chess();
    
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
        
        console.log("evaler: " + line);
        
       
    }

    engine.onmessage = function(event) {
        var line;
        
        if (event && typeof event === "object") {
            line = event.data;
        } else {
            line = event;
        }
        console.log("Reply: " + line)
       
         var match = line.match(/^bestmove ([a-h][1-8])([a-h][1-8])([qrbn])?/);
           
            if(match) {
               console.log("done!")
			   analyzePrevMove();
            }
            
            
    };
	function analyzePrevMove() {	
		var ret = game.undo()
		if (ret != null) {
			console.log("Analyzing fen: " + game.fen())
			uciCmd('position fen '+game.fen());
			uciCmd('position fen '+game.fen(), evaler);
			uciCmd('eval');
			uciCmd('go depth 2');
		} else {
			console.log("Game analysis over")
		}
		
	}
    function uciCmd(cmd, which) {
        console.log("UCI: " + cmd);
        
        (which || engine).postMessage(cmd);
    }
	
	var pgnfiletext = document.getElementById('ms_word_filtered_html').value	
	var games = pgnfiletext.split("[Event")
	var game1 = "[Event" +games[1] 
	game.load_pgn(game1)
	//console.log(game.pgn())
	uciCmd('uci');
	uciCmd('ucinewgame');
    uciCmd('isready');
	console.log("Analyzing fen: " + game.fen())
	uciCmd('position fen '+game.fen());
    uciCmd('position fen '+game.fen(), evaler);
	uciCmd('eval');
	uciCmd('go depth 2');
}
