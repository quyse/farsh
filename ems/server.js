var express = require('express');
var fs = require('fs');

var initialize = function(app) {
	app.configure(function() {
		app.use(express.logger());
		app.use(express.favicon());
		app.use(express.bodyParser());
		app.use(express.cookieParser());
		app.use(app.router);
		app.use('/debug', express.static(__dirname + '/../debug', {
			maxAge: 0
		}));
		app.use('/debug/assets', express.static(__dirname + '/../assets', {
			maxAge: 0
		}));
		app.use('/debug', express.static(__dirname + '/site', {
			maxAge: 0
		}));
		app.use('/release', express.static(__dirname + '/../release', {
			maxAge: 0
		}));
		app.use('/release/assets', express.static(__dirname + '/../assets', {
			maxAge: 0
		}));
		app.use('/release', express.static(__dirname + '/site', {
			maxAge: 0
		}));
	});

	return app;
};

initialize(express()).listen(8080, '0.0.0.0');
