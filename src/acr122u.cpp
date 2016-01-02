#ifndef BUILDING_NODE_EXTENSION
# define BUILDING_NODE_EXTENSION
#endif

#include <node/v8.h>
#include <node/node.h>
#include <node/node_object_wrap.h>

#include <unistd.h>

using namespace v8;

#include "acr122u.h"

const uint8_t uiPollNr = 20;
const uint8_t uiPeriod = 2;

v8::Persistent< v8::Function > Acr122U::constructor;

void Acr122U::Init( v8::Handle<v8::Object> exports )
{
	v8::Isolate* isolate = v8::Isolate::GetCurrent();
	
	// Prepare constructor template
	v8::Local<FunctionTemplate> tpl = v8::FunctionTemplate::New(isolate, New);
	tpl->SetClassName(v8str("Acr122U"));
	tpl->InstanceTemplate()->SetInternalFieldCount(1);

	NODE_SET_PROTOTYPE_METHOD(tpl, "pump", Pump);
	NODE_SET_PROTOTYPE_METHOD(tpl, "close", Close);
	
	constructor.Reset(isolate, tpl->GetFunction());
	exports->Set(v8str("Acr122U"), tpl->GetFunction());
}

void Acr122U::New(const v8::FunctionCallbackInfo<v8::Value>& args) {
	v8::Isolate* isolate = v8::Isolate::GetCurrent();
	v8::HandleScope scope(isolate);

	if( !args.IsConstructCall() )
	{
		// Invoked as plain function `MyObject(...)`, turn into construct call.
		const int argc = 1;
		v8::Local<v8::Value> argv[argc] = { args[0] };
		v8::Local<v8::Function> cons = v8::Local<v8::Function>::New(isolate, constructor);
		args.GetReturnValue().Set(cons->NewInstance(argc, argv));
		return;
	}

	//Invoked as constructor: `new MyObject(...)`
	if (args.Length() < 1) {
		isolate->ThrowException(v8::Exception::TypeError(v8str("Wrong number of arguments.")));
		return;
	}

	Acr122U *obj = new Acr122U();
	if( !obj->initialise( args ) )
	{
		delete obj;
		return;
	}

	obj->Wrap(args.This());
	args.GetReturnValue().Set(args.This());
}

bool Acr122U::initialise( const v8::FunctionCallbackInfo<v8::Value>& args )
{
	v8::Isolate* isolate = v8::Isolate::GetCurrent();
	v8::HandleScope scope(isolate);

	v8::Local< v8::String > fieldName = args[0]->ToString();
	char fieldChar[ fieldName->Length() + 1 ];
	fieldName->WriteUtf8( fieldChar );

	Nfc::nfc_init(&m_context);
	if( !m_context ) {
		isolate->ThrowException( v8::Exception::TypeError( v8str("Unable to initialise libnfc") ) );
		return false;
	}

	// Try a few times:
	m_pnd = NULL;
	for( int tries=0; tries < 10 && !m_pnd; tries++ )
	{
		m_pnd = Nfc::nfc_open(m_context, fieldChar);
		if( !m_pnd )
			// Wait a second, then try again:
			sleep(1);
	}

	if( !m_pnd )
	{
		isolate->ThrowException( v8::Exception::TypeError( v8str("Unable to open NFC device") ) );
		Nfc::nfc_exit(m_context);
		return false;
	}

	return true;
}

void Acr122U::Close( const v8::FunctionCallbackInfo<Value>& args )
{
	v8::Isolate* isolate = v8::Isolate::GetCurrent();
	v8::HandleScope scope(isolate);

	Acr122U* parent = Unwrap<Acr122U>(args.Holder());
	parent->m_running = false;
	uv_thread_join( &parent->m_thread );
	uv_close( (uv_handle_t*)&parent->m_async, NULL );
}

void Acr122U::Pump( const v8::FunctionCallbackInfo<Value>& args )
{
	v8::Isolate* isolate = v8::Isolate::GetCurrent();
	v8::HandleScope scope(isolate);

	Acr122U* parent = Unwrap<Acr122U>(args.Holder());
	if( args.Length() > 0 )
	{
		v8::Local<v8::Function> callbackHandle = args[0].As<Function>();
		parent->m_data = new Nan::Callback(callbackHandle);
	}

	parent->m_running = true;

	uv_async_init( uv_default_loop(), &parent->m_async, _Callback );
	parent->m_async.data = (void*)parent;

	uv_thread_create( &parent->m_thread, _Pump, parent );
}

void Acr122U::_Pump(void *arg)
{
	Acr122U *e = static_cast<Acr122U *>(arg);
	if( Nfc::nfc_initiator_init(e->m_pnd) < 0 )
	{
		Nfc::nfc_perror(e->m_pnd, "nfc_initiator_init");
		e->m_running = false;
		return;
	}

	Nfc::nfc_target nt;
	int res;
	do {
		Nfc::nfc_modulation modList;
		modList.nmt = Nfc::NMT_ISO14443A;
		modList.nbr = Nfc::NBR_106;
		if( ( res = Nfc::nfc_initiator_poll_target(e->m_pnd, &modList, 1, uiPollNr, uiPeriod, &nt) ) < 0 )
		{
			if( res == NFC_ENOTSUCHDEV )
				break;

			continue;
		}

		if( nt.nm.nmt == Nfc::NMT_ISO14443A )
		{
			char uid[64];
			uint8_t x, y=0;

			for( x=0; x < nt.nti.nai.szUidLen; x++ )
			{
				if( x > 0 )
					uid[y++] = ':';
				y += snprintf( &uid[y], sizeof(uid)-y-1, "%x", nt.nti.nai.abtUid[x] );
			}
			uid[y] = '\0';

			// Read it.
			if( (nt.nti.nai.btSak & 0x08) == 0 )
				printf("Warning: tag is probably not a Mifare Classic!\n");

			// Enqueue the event:
			nfc_event *n = new nfc_event;
			n->next = NULL;
			memcpy( &n->uid, &uid, sizeof(uid) );

			if( !e->m_eventQueue )
				e->m_eventQueue = n;
			else
			{
				nfc_event *c = e->m_eventQueue;
				while( c && c->next ) c = c->next;
				c->next = n;
			}

			// Notify the event loop:
			uv_async_send( &e->m_async );
		}
	} while( e->m_running && ( res == 1 || res == NFC_ETIMEOUT ) );

	e->m_running = 0;
	return;
}

void Acr122U::_Callback(uv_async_t *uv)
{
	v8::Isolate* isolate = v8::Isolate::GetCurrent();
	v8::HandleScope scope(isolate);
	Acr122U *e = static_cast<Acr122U *>(uv->data);
	
	nfc_event *queue = e->m_eventQueue;
	e->m_eventQueue = NULL;

	if( !e->m_data )
		return;

	// Not the fastest way:
	int length = 0;
	for( nfc_event *c=queue; c; c=c->next ) length++;

	// Build result array:
	v8::Local<v8::Array> results = v8::Array::New(isolate, length);

	int idx = 0;
	for( nfc_event *c=queue; c; c=c->next )
	{
		results->Set( idx++, v8str( c->uid ) );
	}

	const int argc = 1;
	v8::Local<v8::Value> argv[argc] = { results };

	Nan::MakeCallback(Nan::GetCurrentContext()->Global(), e->m_data->GetFunction(), argc, argv);

	nfc_event *n = queue;
	for( nfc_event *c=n; c; c=n )
	{
		n = c->next;
		delete c;
	}
}

Acr122U::Acr122U() :
	m_eventQueue(NULL),
	m_pnd(NULL),
	m_context(NULL),
	m_data(NULL),
	m_running(false)
{
}

Acr122U::~Acr122U()
{
	if( m_running )
	{
		m_running = false;
		uv_thread_join( &m_thread );
		uv_close( (uv_handle_t*)&m_async, NULL );
	}

	if( m_pnd )
	{
		nfc_close(m_pnd);
		nfc_exit(m_context);

		delete m_data;
	}
}

// "Actual" node init:
static void init(v8::Handle<v8::Object> exports) {
	Acr122U::Init(exports);
}

// @see http://github.com/ry/node/blob/v0.2.0/src/node.h#L101
NODE_MODULE(nodeacr122u, init);
