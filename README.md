# SearchFS
File server for searches using various backends

## Building:

This requires plan9port or a plan9 installation to build and run.
 
POSIX:

	make sure PLAN9 is set, and that PLAN9/bin is set in your PATH variable.
	Simply run `mk`, and the program will be available as 0.searchfs
 
plan9:

	mk install


## Layout:

```

# Some of these handlers don't exist, possibly won't exist. PRs are definitely welcome
# For inspiration or guidance on how to create these, look to https://github.com/halfwit/dsearch
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

searchfs [ -m mtpt ] [ -S srv ] -d handlers
 -m Mountpoint for the server
 -S Service (Plan9 only) post service 
 -d directory containing handlers 

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

$ cat <dir>/dict/'someword'
	Some result from dict.org
	http://the/url/of/the/data

$ cat <dir>/lyrics/'Aqua - Barbie Girl'
	Hiya Barbie! Hi Ken!
	[...]

$ cat <dir>/youtube/channel/feed/Level1techs
	Level1Techs - https://youtube.com/feeds/video.xml?channel_id=UC4w1YQAJMWOz4qtxinq55LQ

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

```

## Handlers:

Really, dropping any executable, which takes command line arguments, and writes to stdout into your handlers dir should work. For things that need command line flags, write a small rc script which calls the underlying binary with the flags you require, make the script executable, and put that script into the handler dir instead.

```

#!/bin/rc

ping -c 1 $1

```

Handlers really shine when they wrap API endpoints. Searching a service can be arduous on a web browser, where your favorite search provider may well not index the content usefully, be difficult to navigate, or otherwise garner much more of your time than is necessary. 

[ytcli](https://github.com/halfwit/ytcli) [gcli](https://github.com/halfwit/gcli) [wkcli](https://github.com/halfwit/wkcli) are required to interact with the default handlers!

## Exporting as a network service

```
# Assuming you ran searchfs -S search -d handlers
# Modify 192.168.1.10 to match your local machine that is running searchfs

# On the host machine, listen on the network for incoming connections 
aux/listen1 tcp!*!12345 /bin/exportfs -S /srv/search

# On the client machine, connect to our new listener and mount it to /mnt/search
srv -c tcp!192.168.1.10!19191 search /mnt/search
```
