exports.configureCompiler = function(objectFile, compiler) {
	// объектные файлы: <conf>/<api>-object
	var a = /^([^\/]+)\/([^\/]+)$/.exec(objectFile);
	compiler.configuration = a[1];
	compiler.setSourceFile(a[2].replace(/\./g, '/') + '.cpp');
	compiler.addIncludeDir('../inanity/deps/bullet/repo/src');
	compiler.addIncludeDir('../inanity/deps/freetype/repo/include');
	compiler.addIncludeDir('../inanity/deps/harfbuzz/generated');
};

var staticLibraries = [
	'libinanity-platform',
	{ lib: 'libinanity-graphics-dx11', platform: 'win32' },
	'libinanity-graphics-gl',
	'libinanity-graphics-render',
	'libinanity-graphics-raw',
	'libinanity-platform',
	'libinanity-platform-filesystem',
	'libinanity-graphics-shaders',
	'libinanity-al',
	'libinanity-audio',
	'libinanity-input',
	'libinanity-bullet',
	'libinanity-physics',
	'libinanity-gui',
	'libinanity-crypto',
	'libinanity-sqlitefs',
	'libinanity-sqlite',
	'libinanity-data',
	'libinanity-lua',
	'libinanity-base'
];
var staticDepsLibraries = [
	{ dir: 'lua', lib: 'liblua' },
	{ dir: 'bullet', lib: 'libbullet-dynamics' },
	{ dir: 'bullet', lib: 'libbullet-collision' },
	{ dir: 'bullet', lib: 'libbullet-linearmath' },
	{ dir: 'libpng', lib: 'libpng' },
	{ dir: 'zlib', lib: 'libz' },
	{ dir: 'glew', lib: 'libglew', platform: 'win32' },
	{ dir: 'glew', lib: 'libglew', platform: 'linux' },
	{ dir: 'glew', lib: 'libglew', platform: 'freebsd' },
	{ dir: 'sqlite', lib: 'libsqlite', platform: 'win32' },
	{ dir: 'sqlite', lib: 'libsqlite', platform: 'linux' },
	{ dir: 'sqlite', lib: 'libsqlite', platform: 'freebsd' },
	{ dir: 'freetype', lib: 'libfreetype' },
	{ dir: 'harfbuzz', lib: 'libharfbuzz' },
	{ dir: 'ucdn', lib: 'libucdn' }
];
var dynamicLibraries = {
	win32: [
		'user32.lib', 'gdi32.lib', 'opengl32.lib', 'openal32.lib'
	],
	linux: [
		'pthread', 'GL', 'dl', 'z', 'SDL2', 'openal'
	],
	freebsd: [
		'pthread', 'GL', 'X11', 'z', 'xcb', 'X11-xcb', 'SDL2'
	],
	emscripten: [
		'GL'
	]
};

exports.configureLinker = function(executableFile, linker) {
	var a = /^(([^\/]+)\/)[^\/]+$/.exec(executableFile);
	linker.configuration = a[2];

	var objects = ['main', 'meta', 'Geometry', 'GeometryFormats', 'Material', 'Painter', 'Game', 'Skeleton', 'BoneAnimation'];
	for ( var i = 0; i < objects.length; ++i)
		linker.addObjectFile(a[1] + objects[i]);

	for(var i = 0; i < staticLibraries.length; ++i) {
		var lib = undefined;
		if(typeof staticLibraries[i] == 'string')
			lib = staticLibraries[i];
		else if(staticLibraries[i].platform == linker.platform)
			lib = staticLibraries[i].lib;
		if(lib)
			linker.addStaticLibrary('../inanity/' + a[1] + lib);
	}
	for(var i = 0; i < staticDepsLibraries.length; ++i)
		if(linker.platform == (staticDepsLibraries[i].platform || linker.platform))
			linker.addStaticLibrary('../inanity/deps/' + staticDepsLibraries[i].dir + '/' + a[1] + staticDepsLibraries[i].lib);

	var dl = dynamicLibraries[linker.platform];

	for(var i = 0; i < dl.length; ++i)
		linker.addDynamicLibrary(dl[i]);
};
