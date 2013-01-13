exports.configureCompiler = function(objectFile, compiler) {
	// объектные файлы: <conf>/object
	var a = /^([^\/]+)\/([^\/]+)$/.exec(objectFile);
	compiler.configuration = a[1];
	compiler.setSourceFile(a[2].replace(/\./g, '/') + '.cpp');
};

var staticLibraries = [
	'libinanity-base',
	'libinanity-compress',
	'libinanity-win32window',
	'libinanity-graphics',
	'libinanity-shaders',
	'libinanity-dx',
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
	{ dir: 'bullet', lib: 'libbullet-dynamics' }
	];
var dynamicLibraries =
	'user32.lib gdi32.lib dxgi.lib d3d11.lib d3dx11.lib'.split(' ');

exports.configureLinker = function(executableFile, linker) {
	var a = /^(([^\/]+)\/)([^\/]+)$/.exec(executableFile);
	linker.configuration = a[2];

	var objects = ['main', 'Painter', 'ShaderCache'];
	for ( var i = 0; i < objects.length; ++i)
		linker.addObjectFile(a[1] + objects[i]);

	for(var i = 0; i < staticLibraries.length; ++i)
		linker.addStaticLibrary('../inanity2/' + a[1] + staticLibraries[i]);
	for(var i = 0; i < staticDepsLibraries.length; ++i)
		linker.addStaticLibrary('../inanity2/deps/' + staticDepsLibraries[i].dir + '/' + a[1] + staticDepsLibraries[i].lib);

	for(var i = 0; i < dynamicLibraries.length; ++i)
		linker.addDynamicLibrary(dynamicLibraries[i]);
};
