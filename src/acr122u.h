//#include <pthread.h>

namespace Nfc {
 extern "C" {
# include <nfc/nfc.h>
# include <nfc/nfc-types.h>
 }
}
#include <node/uv.h>
#include <nan.h>

#define v8str(a) v8::String::NewFromUtf8(isolate, a)
#define v8num(a) v8::Number::New(isolate, a)

typedef struct _nfc_event {
	struct _nfc_event	*next;
	char			uid[64];
} nfc_event;

class Acr122U : public node::ObjectWrap {
	static v8::Persistent< v8::Function > constructor;

        protected:
		uv_thread_t m_thread;
		uv_async_t m_async;

		nfc_event		*m_eventQueue;

		struct Nfc::nfc_device 	*m_pnd;
		struct Nfc::nfc_context	*m_context;
		Nan::Callback		*m_data;
		bool			m_running;

        public:
                Acr122U();
                ~Acr122U();

		bool initialise( const v8::FunctionCallbackInfo< v8::Value > &args );
	
		// v8 stuff:
                static void Init(v8::Handle<v8::Object> exports);
                static void New(const v8::FunctionCallbackInfo< v8::Value >& args);
		static void Close(const v8::FunctionCallbackInfo< v8::Value >& args);
		static void Pump(const v8::FunctionCallbackInfo< v8::Value >& args);
		static void _Pump(void *arg);
		static void _Callback( uv_async_t *async );
};

