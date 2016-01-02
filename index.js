var fs = require('fs');
var Acr122U = require('./build/Release/acr122u.node');
var factory = {
	'create': function()
	{
		var self = this;
		var obj = {
			'stream': false,
			'ev': null,
			'options': {
				'flags': 'r',
				'encoding': null,
				'fd': null,
				'autoClose': true
			},

			'data': function( obj )
			{
				// No-op by default. Set this yourself on the returned object.
			},

			'open': function(path, callback)
			{
				try
				{
					this.ev = new Acr122U.Acr122U( path );
					if( callback )
						callback(this);

					this.ev.pump(this.data);
				} catch(e) {
					if( callback )
						callback(this, e);
				}
				return obj;
			},

			'close': function()
			{
				if( !this.ev ) return;

				this.ev.close();
				delete this.ev;
			}
		};
		
		return obj;
	}
};
module.exports = factory;
