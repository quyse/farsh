#!/bin/sh

if NOPROGRESS=1 ice-cpp.bat exe:release/F.A.R.S.H
then
	echo Uploading...
	cp -f box.geo.indices box.geo.vertices knot.geo.vertices knot.geo.indices zombi.geo.vertices zombi.geo.indices zombi.skeleton zombi.ba og.ba og.skeleton og.geo.indices og.geo.vertices axe.ba axe.geo.indices axe.geo.vertices diffuse.jpg specular.jpg labyrint.txt shaders release/F.A.R.S.H.exe ../../drive/F.A.R.S.H.\ Release
fi
