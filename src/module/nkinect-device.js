import bindings from "bindings";
import {NKinectDepthStream, NKinectVideoStream} from "./nkinect-streams";

export const NKINECT_CAPABILITIES = {
    'MOTOR': 0x01,
    'CAMERA': 0x02,
    'AUDIO': 0x04
}

export const NKINECT_RESOLUTION = {
    'LOW': 0x0,
    'MEDIUM': 0x1,
    'HIGH': 0x2,
    'DUMMY': 2147483647
}

export const NKINECT_VIDEO_FORMAT = {
    'RGB': 0x00,
    'BAYER': 0x01,
    'IR_8BIT': 0x02,
    'IR_10BIT': 0x03,
    'IR_10BIT_PACKED': 0x04,
    'YUV_RGB': 0x05,
    'YUV_RAW': 0x06,
    'DUMMY': 2147483647
}

export const NKINECT_DEPTH_FORMAT = {
    'F11BIT': 0x00,
    'F10BIT': 0x01,
    'F11BIT_PACKED': 0x02,
    'F10BIT_PACKED': 0x03,
    'REGISTERED': 0x04,
    'MM': 0x05,
    'DUMMY': 2147483647
}

export const NKINECT_LED_OPTIONS = {
    'OFF': 0x00,
    'GREEN': 0x01,
    'RED': 0x02,
    'YELLOW': 0x03,
    'BLINK_GREEN': 0x04,
    'BLINK_RED_YELLOW': 0x06
}

export const NKINECT_TILT_STATUS = {
    'STOPPED': 0x00,
    'LIMIT': 0x01,
    'MOVING': 0x04
}

export const NKINECT_LOGLEVEL = {
    'FATAL': 0x00,
    'ERROR': 0x01,
    'WARNING': 0x02,
    'NOTICE': 0x03,
    'INFO': 0x04,
    'DEBUG': 0x05,
    'SPEW': 0x06,
    'FLOOD': 0x07
}

export default class NKinectDevice {
    static get CAPABILITIES() {
        return NKINECT_CAPABILITIES
    }
    static get RESOLUTION() {
        return NKINECT_RESOLUTION
    }
    static get VIDEO_FORMAT() {
        return NKINECT_VIDEO_FORMAT
    }
    static get DEPTH_FORMAT() {
        return NKINECT_DEPTH_FORMAT
    }
    static get LED_OPTIONS() {
        return NKINECT_LED_OPTIONS
    }
    static get TILT_STATUS() {
        return NKINECT_TILT_STATUS
    }
    static get LOGLEVEL() {
        return NKINECT_LOGLEVEL
    }
    get sending() {
        return this._context.running;
    }
    get running() {
        return this._context.sending;
    }
    constructor(options){
        this._options = {
            device: 0,
            auto: true,
            maxTiltAngle: 31,
            minTiltAngle: -31,
            loglevel: NKinectDevice.LOGLEVEL.FATAL,
            capabilities: NKinectDevice.CAPABILITIES.MOTOR | NKinectDevice.CAPABILITIES.CAMERA,
            ...(typeof(options) === 'number' ? {device: options} : options || {})
        };
        this._context = bindings('nkinect').NKinect(options);
    }
    resume(){
      this._context.resume();
      return this;
    }
    pause(){
        this._context.pause();
        return this;
    }
    setTiltAngle(angle){
        this._context.setTiltAngle(Math.min(this._options.maxTiltAngle, 31, Math.max(this.minTiltAngle, -31, angle)));
        return this;
    }
    getTiltAngle(){
        return this._context.getTiltAngle();
    }
    setLedStatus(status){
        this._context.setLedStatus(status);
        return this;
    }
    getLedStatus(){
        return this._context.getLedStatus();
    }
    startVideo(options, cb){
        this._context.startVideo(this._options.video = {
          resolution: NKinectDevice.RESOLUTION.MEDIUM,
          format: NKinectDevice.VIDEO_FORMAT.RGB,
          ...options}, cb || options);
        return this;
    }
    stopVideo(){
        delete this._options.video;
        this._context.stopVideo();
        return this;
    }
    startDepth(options, cb){
        this._context.startDepth(this._options.depth = {
          resolution: NKinectDevice.RESOLUTION.MEDIUM,
          format: NKinectDevice.VIDEO_DEPTH.F11BIT,
          ...options}, cb || options);
        return this;
    }
    stopDepth(){
        delete this._options.depth;
        this._context.stopDepth();
        return this;
    }
}
