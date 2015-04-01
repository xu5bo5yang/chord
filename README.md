'query'(query.c sha1.c sha1.h Makefile(query)):
	Dealing with all the query result, showing the result as the document shows.
	How to use: just follow the instruction showed on terminal.
	
'chord'(chord.c chord.h sha1.c sha1.h Makefile):
	Accomplish the following functions:
		create new node;
		join one node to an existing node;
		maintain ring & finger table;
		close an existing node.
	
How to use:
		to create a new node with port 8080: chord 8080;
		to join a node 8001 to 8080: chord 8001 127.0.0.1 8080;
		maintain ring & finger table: automatically;
		close an existing node: kill or ctrl+c
