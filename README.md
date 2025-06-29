# Veqi's Blocker
This is blocker by veqi

*Is a fork of google block by [MyNewBie](https://github.com/Pointer31)*\
*Included maps are all redesigned by **veqi** with **original** authors in credits*\
*Recomended to see [their](https://github.com/MyNewBie/blocker-mod-src/tree/master) readme*

Building
--------

DDNet requires additional libraries, that are bundled for the most common platforms (Windows, Mac, Linux, all x86 and x86_64). Instead you can install these libraries on your system, remove the `config.lua` and `bam` should use the system-wide libraries by default. You can install all required dependencies and bam on Debian and Ubuntu like this:

    apt-get install libsdl2-dev libfreetype6-dev libcurl4-openssl-dev libogg-dev libopus-dev libopusfile-dev bam

If you have the libraries installed, but still want to use the bundled ones instead, you can specify so by running `bam config curl.use_pkgconfig=false opus.use_pkgconfig=false opusfile.use_pkgconfig=false ogg.use_pkgconfig=false`.

The MySQL server is not included in the binary releases and can be built with `bam server_sql_release`. It requires `libmariadbclient-dev`, `libmysqlcppconn-dev` and `libboost-dev`, which are also bundled for the common platforms.

Note that the bundled MySQL libraries might not work properly on your system. If you run into connection problems with the MySQL server, for example that it connects as root while you chose another user, make sure to install your system libraries for the MySQL client and C++ connector. Make sure that `mysql.use_mysqlconfig` is set to `true` in your config.lua.

License
------------------------------------------------------------------------

*Teeworlds Copyright (C) 2007-2014 Magnus Auvinen*

*DDRace    Copyright (C) 2010-2011 Shereef Marzouk*

*DDNet     Copyright (C) 2013-2015 Dennis Felsing*

*This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any damages
arising from the use of this software.*

*Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:*

*1. The origin of this software must not be misrepresented; you must not
  claim that you wrote the original software. If you use this software
  in a product, an acknowledgment in the product documentation would be
  appreciated but is not required.*

*2. Altered source versions must be plainly marked as such, and must not be
  misrepresented as being the original software.*

*3. This notice may not be removed or altered from any source distribution.*

*IMPORTANT NOTE! The source under src/engine/external are stripped
libraries with their own licenses. Mostly BSD or zlib/libpng license but
check the individual libraries.*
