exports.configureCompiler = function(objectFile, compiler) {
	// объектные файлы: <conf>/object
	var a = /^([^\/]+)\/([^\/]+)$/.exec(objectFile);
	compiler.configuration = a[1];
	compiler.setSourceFile(a[2].replace(/\./g, '/') + '.cpp');
};

var staticLibraries =
	'libinanity-base libinanity-compress libinanity-win32window libinanity-graphics libinanity-shaders libinanity-dx libinanity-lua libinanity-input'.split(' ');
var dynamicLibraries =
	'user32.lib gdi32.lib dxgi.lib d3d11.lib d3dx11.lib'.split(' ');

exports.configureLinker = function(executableFile, linker) {
	var a = /^(([^\/]+)\/)([^\/]+)$/.exec(executableFile);
	linker.configuration = a[2];

	var objects = ['main'];
	for ( var i = 0; i < objects.length; ++i)
		linker.addObjectFile(a[1] + objects[i]);

	for(var i = 0; i < staticLibraries.length; ++i)
		linker.addStaticLibrary('../inanity2/' + a[1] + staticLibraries[i]);
	linker.addStaticLibrary('../inanity2/deps/lua/' + a[1] + 'liblua');

	for(var i = 0; i < dynamicLibraries.length; ++i)
		linker.addDynamicLibrary(dynamicLibraries[i]);
};
