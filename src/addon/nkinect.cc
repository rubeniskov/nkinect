#include <nan.h>
#include <iostream>
#include <unistd.h>

extern "C" {
  #include <libfreenect/libfreenect.h>
}

class NKinect : public Nan::ObjectWrap {
public:
bool running_ = false;
bool sending_ = false;
freenect_device*       device_;
freenect_context*      context_;
freenect_frame_mode depthMode_;
freenect_frame_mode videoMode_;
uv_async_t uv_async_video_callback_;
uv_async_t uv_async_depth_callback_;
uint8_t *videoBuffer_;
uint8_t *depthBuffer_;
//v8::Buffer*               videoBuffer_;
//v8::Handle<v8::Value>         videoBufferPersistentHandle_;
//v8::Buffer*               depthBuffer_;
//v8::Handle<v8::Value>         depthBufferPersistentHandle_;
uv_loop_t *loop = uv_default_loop(); // TODO
uv_thread_t event_thread_;
Nan::Callback *callback_video;
Nan::Callback *callback_depth;
static NAN_MODULE_INIT(Init) {
        v8::Local<v8::FunctionTemplate> tpl = Nan::New<v8::FunctionTemplate>(New);
        tpl->SetClassName(Nan::New("NKinect").ToLocalChecked());
        tpl->InstanceTemplate()->SetInternalFieldCount(1);

        Nan::SetPrototypeMethod(tpl, "getHandle", GetHandle);
        Nan::SetPrototypeMethod(tpl, "getValue", GetValue);
        Nan::SetPrototypeMethod(tpl, "titlAngle", TitlAngle);
        Nan::SetPrototypeMethod(tpl, "startVideo", StartVideo);
        Nan::SetPrototypeMethod(tpl, "startDepth", StartDepth);
        Nan::SetPrototypeMethod(tpl, "resume", Resume);
        Nan::SetPrototypeMethod(tpl, "pause", Pause);

        constructor().Reset(Nan::GetFunction(tpl).ToLocalChecked());
        Nan::Set(target, Nan::New("NKinect").ToLocalChecked(),
                 Nan::GetFunction(tpl).ToLocalChecked());
}

void StartDepthCapture(const Nan::FunctionCallbackInfo<v8::Value>& info) {

        this->callback_depth = new Nan::Callback(info[0].As<v8::Function>());

        depthMode_ = freenect_find_depth_mode(FREENECT_RESOLUTION_MEDIUM, FREENECT_DEPTH_11BIT);
        if(!depthMode_.is_valid) {
                Nan::ThrowError("Invalid depth configuration\n");
                return;
        }

        if (freenect_set_depth_mode(this->device_, depthMode_) != 0) {
                Nan::ThrowError("Error setting depth mode\n");
                return;
        };

        freenect_set_depth_callback(this->device_, depth_cb);

        depthBuffer_ = (uint8_t*)malloc(depthMode_.bytes);
        //depthBuffer_ = v8::Buffer::New(depthMode_.bytes);
        //depthBufferPersistentHandle_ = v8::Persistent<v8::Value>::New(depthBuffer_->handle_);

        if (freenect_set_depth_buffer(this->device_, /*v8::Buffer::Data(*/ depthBuffer_ /*)*/) != 0) {
                Nan::ThrowError("Error setting depth buffer\n");
                return;
        };

        std::cout << "Before Depth Start" << std::endl;
        if (freenect_start_depth(this->device_) != 0) {
                Nan::ThrowError("Error starting depth\n");
                return;
        }

        // uv_loop_t *loop = uv_default_loop();
        uv_async_init(this->loop, &this->uv_async_depth_callback_, async_depth_callback);
}

void StopDepthCapture(){
        freenect_stop_depth(this->device_);
}

void Tilt(const double angle) {
        freenect_set_tilt_degs(this->device_, angle);
}

void StartVideoCapture(const Nan::FunctionCallbackInfo<v8::Value>& info) {

        this->callback_video = new Nan::Callback(info[0].As<v8::Function>());

        videoMode_ = freenect_find_video_mode(FREENECT_RESOLUTION_MEDIUM, FREENECT_VIDEO_RGB);
        if(!videoMode_.is_valid) {
                Nan::ThrowError("Invalid video configuration\n");
                return;
        }

        if (freenect_set_video_mode(this->device_, videoMode_) != 0) {
                Nan::ThrowError("Error setting video mode\n");
                return;
        };

        freenect_set_video_callback(this->device_, NKinect::video_cb);

        videoBuffer_ = (uint8_t*)malloc(videoMode_.bytes);
        //videoBuffer_ = v8::Buffer::New(videoMode_.bytes);
        //videoBufferPersistentHandle_ = v8::Persistent<v8::Value>::New(videoBuffer_->handle_);

        if (freenect_set_video_buffer(this->device_, /*v8::Buffer::Data(*/ videoBuffer_ /*)*/) != 0) {
                Nan::ThrowError("Error setting video buffer\n");
                return;
        };

        if (freenect_start_video(this->device_) != 0) {
                Nan::ThrowError("Error starting video\n");
                return;
        }

        // uv_loop_t *loop = uv_default_loop();
        uv_async_init(this->loop, &this->uv_async_video_callback_, async_video_callback);
}

void StopVideoCapture(){
        freenect_stop_video(this->device_);
}

void Resume_(){
        if (!this->running_) {
                this->running_ = true;
                if(uv_thread_create(&this->event_thread_, pthread_callback, (void*)this) != 0) {
                        Nan::ThrowError("Error creating thread\n");
                        return;
                }
        }
}

void Pause_(){
        if (this->running_) {
                this->running_ = false;
                uv_thread_join(&this->event_thread_);
        }
}

void VideoCallback(){
        this->sending_ = true;
        const unsigned argc = 1;
        v8::Isolate * isolate = v8::Isolate::GetCurrent();
        v8::HandleScope handleScope(isolate);
        Nan::MaybeLocal<v8::Object> buffer = Nan::CopyBuffer((char*)videoBuffer_, videoMode_.bytes);
        v8::Local<v8::Value> argv[argc] = { buffer.ToLocalChecked() };
        this->callback_video->Call(argc, argv);
        this->sending_ = false;
}

void DepthCallback(){
        this->sending_ = true;
        const unsigned argc = 1;
        v8::Isolate * isolate = v8::Isolate::GetCurrent();
        v8::HandleScope handleScope(isolate);
        Nan::MaybeLocal<v8::Object> buffer = Nan::CopyBuffer((char*)depthBuffer_, depthMode_.bytes);
        v8::Local<v8::Value> argv[argc] = { buffer.ToLocalChecked() };
        this->callback_depth->Call(argc, argv);
        this->sending_ = false;
}

static void depth_cb(freenect_device *dev, void *depth, uint32_t timestamp)
{
        NKinect* context = (NKinect *) freenect_get_user(dev);
        if (context->sending_) return;
        context->uv_async_depth_callback_.data = (void *) context;
        uv_async_send(&context->uv_async_depth_callback_);
}

static void
async_depth_callback(uv_async_t *handle) {
        NKinect* context = (NKinect *) handle->data;
        context->DepthCallback();
}

static void
async_video_callback(uv_async_t *handle) {
        NKinect* context = (NKinect *) handle->data;
        context->VideoCallback();
}

static void video_cb(freenect_device *dev, void *video, uint32_t timestamp)
{
        NKinect* context = (NKinect *) freenect_get_user(dev);
        if (context->sending_) return;
        context->uv_async_video_callback_.data = (void *) context;
        uv_async_send(&context->uv_async_video_callback_);
}

explicit NKinect(double value = 0) : value_(value) {
        int user_device_number = 0;
        if (freenect_init(&this->context_, NULL) < 0) {
                Nan::ThrowError("Error initializing freenect context");
                return;
        }
        freenect_set_log_level(this->context_, FREENECT_LOG_DEBUG);
        // freenect_set_log_level(this->context_, FREENECT_LOG_SPEW);
        freenect_select_subdevices(this->context_, (freenect_device_flags)(FREENECT_DEVICE_MOTOR | FREENECT_DEVICE_CAMERA));
        int nr_devices = freenect_num_devices (this->context_);
        if (nr_devices < 1) {
                this->Close();
                Nan::ThrowError("No kinect devices present");
                return;
        }

        if (freenect_open_device(this->context_, &this->device_, user_device_number) < 0) {
                this->Close();
                Nan::ThrowError("Could not open device number\n");
                return;
        }

        std::cout << "Initialized" << std::endl;

        freenect_set_user(this->device_, this);


}
~NKinect() {
        ///this->Close();
}

void Close(){
        this->running_ = false;

        if (this->device_ != NULL) {
                if (freenect_close_device(this->device_) < 0) {
                        Nan::ThrowError("Error closing device");
                        return;
                }

                this->device_ = NULL;
        }

        if (this->context_ != NULL) {
                if (freenect_shutdown(context_) < 0) {
                        Nan::ThrowError("Error shutting down");
                        return;
                }

                this->context_ = NULL;
        }
        std::cout << "Shutdown" << std::endl;
}

void ProcessEventsLoop(){
        while(this->running_) {
                //static timeval timeout = { 10, 0 };
                //freenect_process_events_timeout(this->context_, &timeout);
                freenect_process_events(this->context_);
        }
}

void
static pthread_callback(void *user_data) {
        NKinect* kinect = static_cast<NKinect*>(user_data);
        kinect->ProcessEventsLoop();
}
// protected:

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

static NAN_METHOD(GetHandle) {
        NKinect* obj = Nan::ObjectWrap::Unwrap<NKinect>(info.Holder());
        info.GetReturnValue().Set(obj->handle());
}

static NAN_METHOD(GetValue) {
        NKinect* obj = Nan::ObjectWrap::Unwrap<NKinect>(info.Holder());
        info.GetReturnValue().Set(obj->value_);
}
// TODO handle arguments here
static NAN_METHOD(StartVideo) {
        NKinect* obj = Nan::ObjectWrap::Unwrap<NKinect>(info.Holder());
        obj->StartVideoCapture(info);
        info.GetReturnValue().Set(obj->handle());
}
// TODO handle arguments here
static NAN_METHOD(StartDepth) {
        NKinect* obj = Nan::ObjectWrap::Unwrap<NKinect>(info.Holder());
        obj->StartDepthCapture(info);
        info.GetReturnValue().Set(obj->handle());
}

static NAN_METHOD(Resume) {
        NKinect* obj = Nan::ObjectWrap::Unwrap<NKinect>(info.Holder());
        obj->Resume_();
        info.GetReturnValue().Set(obj->handle());
}

static NAN_METHOD(Pause) {
        NKinect* obj = Nan::ObjectWrap::Unwrap<NKinect>(info.Holder());
        obj->Pause_();
        info.GetReturnValue().Set(obj->handle());
}

static NAN_METHOD(TitlAngle) {
        if (info.Length() == 1) {
                if (!info[0]->IsNumber())
                        return Nan::ThrowError("tilt argument must be a number\n");
        } else {
                return Nan::ThrowError("Expecting at least one argument with the led status");
        }

        double angle = info[0]->NumberValue();
        NKinect* obj = Nan::ObjectWrap::Unwrap<NKinect>(info.Holder());
        obj->Tilt(angle);
        info.GetReturnValue().Set(obj->handle());
}

static inline Nan::Persistent<v8::Function> & constructor() {
        static Nan::Persistent<v8::Function> freenect_constructor;
        return freenect_constructor;
}

double value_;
};

NODE_MODULE(objectwrapper, NKinect::Init)
