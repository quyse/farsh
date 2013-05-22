var apiDefines = {
	dx11: 'FARSH_USE_DIRECTX11',
	gl: 'FARSH_USE_OPENGL'
};

exports.configureCompiler = function(objectFile, compiler) {
	// объектные файлы: <conf>/<api>-object
	var a = /^([^\/]+)\/([^\/]+)\-([^\/]+)$/.exec(objectFile);
	compiler.configuration = a[1];
	compiler.setSourceFile(a[3].replace(/\./g, '/') + '.cpp');
	compiler.addMacro(apiDefines[a[2]]);
};

var staticLibraries = [
	'libinanity-base',
	'libinanity-compress',
	'libinanity-win32window',
	'libinanity-graphics',
	'libinanity-shaders',
	{ lib: 'libinanity-dx11', api: 'dx11' },
	{ lib: 'libinanity-gl', api: 'gl' },
	'libinanity-lua',
	'libinanity-input',
	'libinanity-physics',
	'libinanity-bullet',
	'libinanity-crypto',
	'libinanity-sqlitefs'
];
var staticDepsLibraries = [
	{ dir: 'lua', lib: 'liblua' },
	{ dir: 'bullet', lib: 'libbullet-linearmath' },
	{ dir: 'bullet', lib: 'libbullet-collision' },
	{ dir: 'bullet', lib: 'libbullet-dynamics' },
	{ dir: 'libpng', lib: 'libpng' },
	{ dir: 'glew', lib: 'libglew', api: 'gl' }
];
var dynamicLibraries = [
	'user32.lib', 'gdi32.lib', 'dxgi.lib', 'd3d11.lib', 'd3dx11.lib', 'd3dx10.lib',
	{ lib: 'opengl32.lib', api: 'gl' }
];

exports.configureLinker = function(executableFile, linker) {
	var a = /^(([^\/]+)\/)[^\/]+\-([^\/]+)$/.exec(executableFile);
	linker.configuration = a[2];

	var api = a[3];

	var objects = ['main', 'Geometry', 'GeometryFormats', 'Material', 'Painter', 'Game', 'Skeleton', 'BoneAnimation'];
	for ( var i = 0; i < objects.length; ++i)
		linker.addObjectFile(a[1] + api + '-' + objects[i]);

	for(var i = 0; i < staticLibraries.length; ++i) {
		var lib = undefined;
		if(typeof staticLibraries[i] == 'string')
			lib = staticLibraries[i];
		else if(staticLibraries[i].api == api)
			lib = staticLibraries[i].lib;
		if(lib)
			linker.addStaticLibrary('../inanity2/' + a[1] + lib);
	}
	for(var i = 0; i < staticDepsLibraries.length; ++i)
		if(!staticDepsLibraries[i].api || staticDepsLibraries[i].api == api)
			linker.addStaticLibrary('../inanity2/deps/' + staticDepsLibraries[i].dir + '/' + a[1] + staticDepsLibraries[i].lib);

	for(var i = 0; i < dynamicLibraries.length; ++i) {
		var lib = undefined;
		if(typeof dynamicLibraries[i] == 'string')
			lib = dynamicLibraries[i];
		else if(dynamicLibraries[i].api == api)
			lib = dynamicLibraries[i].lib;
		if(lib)
			linker.addDynamicLibrary(lib);
	}
};
