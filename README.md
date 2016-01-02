# nodeacr122u
Lightweight LibNFC glue for Node.js (intended for reading Mifare Classic RFIDs via ACR122U)

If you do intend on using this module with an ACR122U, I recommend fixing your libNFC driver with this patch:

* [https://gist.github.com/danieloneill/3be43d5374c80d89ea73](https://gist.github.com/danieloneill/3be43d5374c80d89ea73)

This patch will fix the driver so it will emit NFC_ETIMEOUT events when there isn't an RFID tag within reader proximity. Without this patch libNFC will simply emit NFC_EIO (General I/O Error), causing the module to exit.

It also [checks packet sizes correctly, which for some reason the libNFC guys haven't yet fixed](http://osdir.com/ml/general/2014-08/msg50964.html).

### Example

```javascript
#!/usr/bin/env node

var Acr122U = require('nodeacr122u');

var dev = Acr122U.create();
dev.open( 'acr122_usb:', function(obj, err) {
	if( err ) {
		console.log("Acr122U: Failed to open device: "+err);
		return false;
	}
	dev['data'] = function(data) {
		console.log("Acr122U event: "+JSON.stringify(data, null, 2));
	};
});

// Hack to prevent Node.js from exiting, sorry.
var http = require('http');
var httpServer = http.createServer(function(){});
httpServer.listen(8084);
console.log("Listening for keyboard input...");
```

# Methods

## Factory

### Acr122u::create()
`Returns an initialised Acr122u object.`

* Returns: An initialised Acr122u object.

---

## Acr122u object

### Acr122uObject::open( path, callback( acr122uObject, error ) )
`Open an Acr122u device, associate it to this object, and fire the provided callback upon completion.`

The parameters passed to the provided callback function are **Acr122uObject** which is a reference to the Acr122u object, and **error** which will be an error string in the case of an error, or undefined on success.

* Returns: A reference to itself.

### Acr122uObject::data( Acr122uEvent )
`A virtual method of the Acr122uObject. Reimplement it to handle Acr122uEvents. It can be ignored, but if you ignore it, you might as well not use this module at all.`

### Acr122uObject::closed( Acr122uObject )
`A virtual method of the Acr122uObject. Reimplement it to react to the device being closed. Otherwise it can be ignored.`

