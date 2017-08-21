import Readable from "readable-stream/readable";

export class NKinectStream extends Readable {
    constructor(options){
        super(options);
        this.on('unpipe', () => {
            if(!this._readableState.pipesCount){
                this._stop();
            }
        });
    }
    _read(){
        this._start();
    }
    _writeData(buff, timestamp){
        if(this._readableState.writableObjectMode || this._readableState.objectMode)
            this.emit('data', {data: buff, timestamp: timestamp});
        else
            this.emit('data', buff);
    }
    _start(){

    }
    _stop(){

    }
}

export class NKinectDepthStream extends NKinectStream {
    constructor(context, options){
        super(options);
        this._options = options;
        this._context = context;
    }
    _start(){
        this._context.startDepth(this._options, (buff, timestamp) => {
            this._writeData(buff, timestamp);
        });
    }
    _stop(){
        this._context.stopDepth();
        this.end();
    }
}

export class NKinectVideoStream extends NKinectStream {
    constructor(context, options){
        super(options);
        this._options = options;
        this._context = context;
    }
    _start(){
        this._context.startVideo(options, (buff, timestamp) => {
            this._writeData(buff, timestamp);
        });
    }
    _stop(){
        this._context.stopVideo();
        this.end();
    }
}
