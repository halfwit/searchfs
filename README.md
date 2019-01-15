# SearchFS
File server for searches using various backends

## Building:

This requires plan9port or a plan9 installation to build and run.
 
POSIX:

	make sure PLAN9 is set, and that PLAN9/bin is set in your PATH variable.
	Simply run `mk`, and the program will be available as 0.searchfs
	TODO: mk install
	This currently builds, but does not mount properly on FreeBSD. This is due to FreeBSD not packaging fusermount along with their fuse implementation.
 
plan9:

	mk install


## Layout:

```

$ ls <dir>/
	google/     # Query Google (normal searches)
	youtube/    # Single videos, playlists, channels, users
	code/       # Search predefined directories for snippets of code
	dictionary/ # Wrap your favorite dict protocol
	documents/  # Search tags for matching (local) documents
	games/      # Search tags for matching games
	github/     # May be abandoned for a dedicated filesystem
	images/     # Multi-service image search
	bookmarks/  # Search tags for matching bookmarks (this will index your bookmark collection)
	lyrics/     # Lyrical snippets returning matching song or artist - title
	quote/      # Your local quote db, saved from IRC or other things
	music/      # Wrapper for your preferred music streaming service - initially just google music
	wikipedia   # May be abandoned for a dedicated filesystem

```

## Usage:

```

searchfs [ -cD ] [ -m mtpt ] [ -s srv] -h handlers
 -c Caches all queries to files in a tree which mirrors searchfs
 -D Debug
 -m Mountpoint for the server
 -s Service (Plan9 only) post service 
 -h directory containing handlers 

```

For each handler, searchfs will create a directory, and listen for file creations and reads.
When data is requested, searchfs will invoke the handler, with the file name as the argument.

```

# This will call the `Google` handler, with the argument `mysearch`
# In turn, that will perform a Google query for the term, listing the results in mysearch.

$ cat <dir>/google/mysearch
	list    http://path.to/some/site
	of      http://path.to/some/other/site
	results http://path.to/some/third/site

$ cat <dir>/dictionary/'someword'
	Some result from dict.org
	http://the/url/of/the/data

$ cat <dir>/lyrics/'Aqua - Barbie Girl'
	Hiya Barbie! Hi Ken!
	[...]


```

## Configuration:

There's not much to configure here. All configuration options are per-handler.
 A typical file looks like this:

```

handler=google
	# Handler specific data here
	maxresults=50
	apikey=myapikey

handler=youtube
	maxresults=50
	apikey=myapikey


## Handlers:

Really, dropping any executable, which takes command line arguments, and writes to stdout into your handlers dir should work. For things that need command line flags, write a small rc script which calls the underlying binary with the flags you require, make the script executable, and put that script into the handler dir instead.

```

#!/bin/rc

ping -c 1 $1

```

Handlers really shine when they wrap API endpoints. Searching a service can be arduous on a web browser, where your favorite search provider may well not index the content usefully, be difficult to navigate, or otherwise garner much more of your time than is necessary. 

[ytcli](https://github.com/halfwit/ytcli) required to use youtube handler