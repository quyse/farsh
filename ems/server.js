var express = require('express');
var fs = require('fs');

var initialize = function(app, secure) {
	app.configure(function() {
		app.use(express.logger());
		app.use(express.favicon());
		app.use(express.bodyParser());
		app.use(express.cookieParser());
		app.use(app.router);
		app.use(express.static(__dirname + '/../debug', {
			maxAge: 0
		}));
		app.use('/assets', express.static(__dirname + '/../assets', {
			maxAge: 0
		}));
		app.use(express.static(__dirname + '/site', {
			maxAge: 0
		}));
	});

	return app;
};

initialize(express(), false).listen(8080);
