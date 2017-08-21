import NKinectDevice from "./nkinect-device";
import {NKinectDepthStream, NKinectVideoStream} from "./nkinect-streams";

export default class NKinect extends NKinectDevice {
    call constructor(options) {
        return new NKinect(options);
    }
    constructor(options){
        super(options);
    }
    createVideoStream(options){
        return new NKinectVideoStream(this, options);
    }
    createDepthStream(options){
        return new NKinectDepthStream(this, options);
    }
}
