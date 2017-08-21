#include <nan.h>
#include <iostream>
#include <unistd.h>

extern "C" {
  #include <libfreenect/libfreenect.h>
}

enum NKinectFrameMode {
        NKinectFrameModeDepth, NKinectFrameModeVideo
};

class NKinect : public Nan::ObjectWrap {
public:
bool running = false;
bool sending = false;
freenect_device*       device;
freenect_context*      context;
freenect_frame_mode videoMode;
freenect_frame_mode depthMode;
uv_async_t uv_async_video_callback;
uv_async_t uv_async_depth_callback;
uint8_t *videoBuffer;
uint8_t *depthBuffer;
uv_loop_t *loop = uv_default_loop();
uv_thread_t event_thread;
Nan::Callback *callback_video;
Nan::Callback *callback_depth;

explicit NKinect(double value = 0) : value_(value) {
        int user_device_number = 0;
        if (freenect_init(&this->context, NULL) < 0) {
                Nan::ThrowError("Error initializing freenect context");
                return;
        }
        freenect_set_log_level(this->context, FREENECT_LOG_DEBUG);
        // freenect_set_log_level(this->context, FREENECT_LOG_SPEW);
        freenect_select_subdevices(this->context, (freenect_device_flags)(FREENECT_DEVICE_MOTOR | FREENECT_DEVICE_CAMERA));
        int nr_devices = freenect_num_devices(this->context);
        if (nr_devices < 1) {
                this->Close();
                Nan::ThrowError("No kinect devices present");
                return;
        }

        if (freenect_open_device(this->context, &this->device, user_device_number) < 0) {
                this->Close();
                Nan::ThrowError("Could not open device number\n");
                return;
        }


        freenect_set_user(this->device, this);


}
~NKinect() {
        this->Close();
}

void DepthCallback(){
        this->sending = true;
        const unsigned argc = 1;
        v8::Isolate * isolate = v8::Isolate::GetCurrent();
        v8::HandleScope handleScope(isolate);
        Nan::MaybeLocal<v8::Object> buffer = Nan::CopyBuffer((char*)this->depthBuffer, this->depthMode.bytes);
        v8::Local<v8::Value> argv[argc] = { buffer.ToLocalChecked() };
        this->callback_depth->Call(argc, argv);
        this->sending = false;
}

void StartDepthCapture(const v8::Local<v8::Function> &callback) {
        return this->StartDepthCapture(callback, Nan::New<v8::Object>());
}

void StartDepthCapture(const v8::Local<v8::Function> &callback, const v8::Local<v8::Object> &options) {
        // Check if previous video capture running settings are the same and ignore stop and initialization
        this->StopDepthCapture();
        this->callback_depth = new Nan::Callback(callback);
        this->depthMode = NKinect::freenect_get_frame_mode_by_options(NKinectFrameModeDepth, options);
        if(!this->depthMode.is_valid) {
                Nan::ThrowError("Invalid depth configuration\n");
                return;
        }

        if (freenect_set_depth_mode(this->device, this->depthMode) != 0) {
                Nan::ThrowError("Error setting depth mode\n");
                return;
        };

        freenect_set_depth_callback(this->device, NKinect::freenect_device_depth_cb);

        this->depthBuffer = (uint8_t*)malloc(this->depthMode.bytes);

        if (freenect_set_depth_buffer(this->device, this->depthBuffer) != 0) {
                Nan::ThrowError("Error setting depth buffer\n");
                return;
        };

        if (freenect_start_depth(this->device) != 0) {
                Nan::ThrowError("Error starting depth\n");
                return;
        }

        uv_async_init(this->loop, &this->uv_async_depth_callback, NKinect::async_depth_callback);
}

void StopDepthCapture(){
        freenect_stop_depth(this->device);
}

void VideoCallback(){
        this->sending = true;
        const unsigned argc = 1;
        v8::Isolate * isolate = v8::Isolate::GetCurrent();
        v8::HandleScope handleScope(isolate);
        Nan::MaybeLocal<v8::Object> buffer = Nan::CopyBuffer((char*)this->videoBuffer, this->videoMode.bytes);
        v8::Local<v8::Value> argv[argc] = { buffer.ToLocalChecked() };
        this->callback_video->Call(argc, argv);
        this->sending = false;
}

void StartVideoCapture(const v8::Local<v8::Function> &callback) {
        return this->StartVideoCapture(callback, Nan::New<v8::Object>());
}

void StartVideoCapture(const v8::Local<v8::Function> &callback, const v8::Local<v8::Object> &options) {
        // Check if previous video capture running settings are the same and ignore stop and initialization
        this->StopVideoCapture();
        this->callback_video = new Nan::Callback(callback);
        this->videoMode = NKinect::freenect_get_frame_mode_by_options(NKinectFrameModeVideo, options);
        // this->videoMode = freenect_find_video_mode(FREENECT_RESOLUTION_MEDIUM, FREENECT_VIDEO_RGB);
        if(!this->videoMode.is_valid) {
                Nan::ThrowError("Invalid video configuration\n");
                return;
        }

        if (freenect_set_video_mode(this->device, this->videoMode) != 0) {
                Nan::ThrowError("Error setting video mode\n");
                return;
        };

        freenect_set_video_callback(this->device, NKinect::freenect_device_video_cb);

        this->videoBuffer = (uint8_t*)malloc(this->videoMode.bytes);

        if (freenect_set_video_buffer(this->device, this->videoBuffer) != 0) {
                Nan::ThrowError("Error setting video buffer\n");
                return;
        };

        if (freenect_start_video(this->device) != 0) {
                Nan::ThrowError("Error starting video\n");
                return;
        }

        uv_async_init(this->loop, &this->uv_async_video_callback, NKinect::async_video_callback);
}

void StopVideoCapture(){
        freenect_stop_video(this->device);
}

void SetTiltAngle(const double angle) {
        freenect_set_tilt_degs(this->device, angle);
}

void Resume(){
        if (!this->running) {
                this->running = true;
                if(uv_thread_create(&this->event_thread, NKinect::pthread_callback, (void*)this) != 0) {
                        Nan::ThrowError("Error creating thread\n");
                        return;
                }
        }
}

void Pause(){
        if (this->running) {
                this->running = false;
                uv_thread_join(&this->event_thread);
        }
}

void Close(){
        this->running = false;

        if (this->device != NULL) {
                if (freenect_close_device(this->device) < 0) {
                        Nan::ThrowError("Error closing device");
                        return;
                }

                this->device = NULL;
        }

        if (this->context != NULL) {
                if (freenect_shutdown(this->context) < 0) {
                        Nan::ThrowError("Error shutting down");
                        return;
                }

                this->context = NULL;
        }
}

void ProcessEventsLoop(){
        while(this->running) {
                //static timeval timeout = { 10, 0 };
                //freenect_process_events_timeout(this->context, &timeout);
                freenect_process_events(this->context);
        }
}

static
void
pthread_callback(void *user_data) {
        NKinect* kinect = static_cast<NKinect*>(user_data);
        kinect->ProcessEventsLoop();
}

static
void
async_video_callback(uv_async_t *handle) {
        NKinect* context = static_cast<NKinect*>(handle->data);
        context->VideoCallback();
}

static
void
async_depth_callback(uv_async_t *handle) {
        NKinect* context = static_cast<NKinect*>(handle->data);
        context->DepthCallback();
}

static
void
freenect_device_video_cb(freenect_device *dev, void *video, uint32_t timestamp)
{
        NKinect* context = static_cast<NKinect*>(freenect_get_user(dev));
        if (context->sending) return;
        context->uv_async_video_callback.data = (void *) context;
        uv_async_send(&context->uv_async_video_callback);
}

static
void
freenect_device_depth_cb(freenect_device *dev, void *depth, uint32_t timestamp)
{
        NKinect* context = static_cast<NKinect*>(freenect_get_user(dev));
        if (context->sending) return;
        context->uv_async_depth_callback.data = (void *) context;
        uv_async_send(&context->uv_async_depth_callback);
}

static
freenect_frame_mode
freenect_get_frame_mode_by_options(NKinectFrameMode mode, const v8::Local<v8::Object> &options){
        v8::Local<v8::Value> fmt = options->Get(Nan::New<v8::String>("format").ToLocalChecked());
        v8::Local<v8::Value> res = options->Get(Nan::New<v8::String>("resolution").ToLocalChecked());

        if(!fmt->IsNumber())
                res = Nan::New<v8::Number>(FREENECT_RESOLUTION_MEDIUM);

        if(mode == NKinectFrameModeDepth){
            if(!fmt->IsNumber())
                    res = Nan::New<v8::Number>(FREENECT_DEPTH_11BIT);
            return freenect_find_depth_mode(static_cast<freenect_resolution>(res->Uint32Value()), static_cast<freenect_depth_format>(fmt->Uint32Value()));
        }

        if(!fmt->IsNumber())
                res = Nan::New<v8::Number>(FREENECT_VIDEO_RGB);
        // printf("as %d", fmt->Uint32Value());
        return freenect_find_video_mode(static_cast<freenect_resolution>(res->Uint32Value()), static_cast<freenect_video_format>(fmt->Uint32Value()));
        // return freenect_find_video_mode(static_cast<freenect_resolution>(res->Uint32Value()), FREENECT_VIDEO_RGB);
}

static NAN_MODULE_INIT(Init) {
        v8::Local<v8::FunctionTemplate> tpl = Nan::New<v8::FunctionTemplate>(New);
        tpl->SetClassName(Nan::New("NKinectDevice").ToLocalChecked());
        tpl->InstanceTemplate()->SetInternalFieldCount(1);

        Nan::SetAccessor(tpl->InstanceTemplate(), Nan::New<v8::String>("running").ToLocalChecked(), getRunning);
        Nan::SetAccessor(tpl->InstanceTemplate(), Nan::New<v8::String>("sending").ToLocalChecked(), getSending);

        Nan::SetPrototypeMethod(tpl, "setTiltAngle", TitlAngle);
        Nan::SetPrototypeMethod(tpl, "setLedStatus", LedStatus);
        Nan::SetPrototypeMethod(tpl, "startVideo", StartVideo);
        Nan::SetPrototypeMethod(tpl, "stopVideo", StopVideo);
        Nan::SetPrototypeMethod(tpl, "startDepth", StartDepth);
        Nan::SetPrototypeMethod(tpl, "stopDepth", StopDepth);
        Nan::SetPrototypeMethod(tpl, "resume", Resume);
        Nan::SetPrototypeMethod(tpl, "pause", Pause);

        constructor().Reset(Nan::GetFunction(tpl).ToLocalChecked());
        Nan::Set(target, Nan::New("NKinect").ToLocalChecked(),
                 Nan::GetFunction(tpl).ToLocalChecked());
}
private:
static NAN_METHOD(New) {
        if (info.IsConstructCall()) {
                double value = info[0]->IsUndefined() ? 0 : Nan::To<double>(info[0]).FromJust();
                NKinect *obj = new NKinect(value);
                obj->Wrap(info.This());
                info.GetReturnValue().Set(info.This());
        } else {
                const int argc = 1;
                v8::Local<v8::Value> argv[argc] = {info[0]};
                v8::Local<v8::Function> cons = Nan::New(constructor());
                info.GetReturnValue().Set(Nan::NewInstance(cons, argc, argv).ToLocalChecked());
        }
}

static NAN_GETTER(getRunning) {
        NKinect* obj = Nan::ObjectWrap::Unwrap<NKinect>(info.Holder());
        info.GetReturnValue().Set(Nan::New<v8::Boolean>(obj->running));
}

static NAN_GETTER(getSending) {
        NKinect* obj = Nan::ObjectWrap::Unwrap<NKinect>(info.Holder());
        info.GetReturnValue().Set(Nan::New<v8::Boolean>(obj->sending));
}

static NAN_METHOD(StartVideo) {
        if (info.Length() < 1)
                return Nan::ThrowError("Expecting at least one argument in StartVideo");
        NKinect* obj = Nan::ObjectWrap::Unwrap<NKinect>(info.Holder());
        if (info.Length() == 1) {
                if(!info[0]->IsFunction())
                        return Nan::ThrowError("Calback argument must be a function in StartVideo");
                obj->StartVideoCapture(info[0].As<v8::Function>());
        } else {
                if(!info[0]->IsObject())
                        return Nan::ThrowError("Options argument must be a object in StartVideo");
                if(!info[1]->IsFunction())
                        return Nan::ThrowError("Calback argument must be a function in StartVideo");
                obj->StartVideoCapture(info[1].As<v8::Function>(), info[0].As<v8::Object>());
        }

        info.GetReturnValue().Set(obj->handle());
}

static NAN_METHOD(StartDepth) {
        if (info.Length() < 1)
                return Nan::ThrowError("Expecting at least one argument in StartDepth");
        NKinect* obj = Nan::ObjectWrap::Unwrap<NKinect>(info.Holder());
        if (info.Length() == 1) {
                if(!info[0]->IsFunction())
                        return Nan::ThrowError("Calback argument must be a function in StartDepth");
                obj->StartDepthCapture(info[0].As<v8::Function>());
        } else {
                if(!info[0]->IsObject())
                        return Nan::ThrowError("Options argument must be a object in StartDepth");
                if(!info[1]->IsFunction())
                        return Nan::ThrowError("Calback argument must be a function in StartDepth");
                obj->StartDepthCapture(info[1].As<v8::Function>(), info[0].As<v8::Object>());
        }

        info.GetReturnValue().Set(obj->handle());
}

static NAN_METHOD(Resume) {
        NKinect* obj = Nan::ObjectWrap::Unwrap<NKinect>(info.Holder());
        obj->Resume();
        info.GetReturnValue().Set(obj->handle());
}

static NAN_METHOD(Pause) {
        NKinect* obj = Nan::ObjectWrap::Unwrap<NKinect>(info.Holder());
        obj->Pause();
        info.GetReturnValue().Set(obj->handle());
}

static NAN_METHOD(StopVideo) {
        NKinect* obj = Nan::ObjectWrap::Unwrap<NKinect>(info.Holder());
        obj->StopVideoCapture();
        info.GetReturnValue().Set(obj->handle());
}

static NAN_METHOD(StopDepth) {
        NKinect* obj = Nan::ObjectWrap::Unwrap<NKinect>(info.Holder());
        obj->StopDepthCapture();
        info.GetReturnValue().Set(obj->handle());
}
static NAN_METHOD(TitlAngle) {
        if (info.Length() == 1) {
                if (!info[0]->IsNumber())
                        return Nan::ThrowError("Tilt argument must be a number\n");
        } else {
                return Nan::ThrowError("Expecting at least one argument with the led status");
        }

        double angle = info[0]->NumberValue();
        NKinect* obj = Nan::ObjectWrap::Unwrap<NKinect>(info.Holder());
        obj->SetTiltAngle(angle);
        info.GetReturnValue().Set(obj->handle());
}
static NAN_METHOD(LedStatus) {
        NKinect* obj = Nan::ObjectWrap::Unwrap<NKinect>(info.Holder());
        info.GetReturnValue().Set(obj->handle());
}
static inline Nan::Persistent<v8::Function> & constructor() {
        static Nan::Persistent<v8::Function> freenect_constructor;
        return freenect_constructor;
}

double value_;
};

NODE_MODULE(objectwrapper, NKinect::Init)
