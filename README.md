SrcTV+
======

Makes SrcTV send everything. For Team Fortress 2. Adds a small amount of extra
network traffic.

Installation
------------

Copy `srctvplus.so`(Linux) or `srctvplus.dll`(Windows) along with `srctvplus.vdf`
into `<TF2>/tf/custom/srctvplus/addons/` or `<TF2>/tf/addons/`.

### Network traffic usage

The plugin works by having the game server send the SrcTV clients more data,
and therefore the default `tv_maxrate 8000` may not be enough in all cases.
This is especially true at high `tv_snapshotrate`. It's therefore recommended
to set at least `tv_maxrate 20000` or if possible, `tv_maxrate 0`.

Compiling
---------

### Linux:
```
$ git submodule init
$ git submodule update
$ ./build.sh
```

### Windows:
Open srctvplus.sln in Visual Studio 2017. Select 'Release' configuration.
Press `F6` to build. Resulting file will be in `Release/srctvplus.dll`.

Note that building requires the Visual Studio 2013 Build Tools(v120).
This is included in 
[Visual Studio 2013 Community Edition](https://go.microsoft.com/fwlink/?LinkId=532495&clcid=0x409).

Credits
-------

  - FunctionRoute(from TFTrue): Didrole
  - Source SDK: By Valve. Packaged by AlliedModders.
