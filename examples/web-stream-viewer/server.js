var kinect = require('../../')();
var jpegJs = require('jpeg-js')
var through = require('through')
var bodyParser = require('body-parser');
var browserify = require('browserify-middleware');
var http = require('http');
var express = require('express');
var ndarray = require('ndarray');
var ops = require('ndarray-ops');
var app = express();

var program = {
  port: 3000,
  host: '0.0.0.0'
}

var width = 640
var height = 480;
var boundaryID = "BOUNDARY";

var reArrangeBuffer = (type) => {
  return through(function(chunk, enc, done) {
    var data = new Buffer(width * height * 4);

    for (var i = 0, depth; i < width * height; i++) {
        data[4*i]   = type === 'depth' ? (depth = (chunk[2*i]+chunk[2*i+1]*255)/5) : chunk[3*i];
        data[4*i+1] = type === 'depth' ? depth : chunk[3*i+1];
        data[4*i+2] = type === 'depth' ? depth : chunk[3*i+2];
        data[4*i+3] = type === 'depth' ? 255 : chunk[3*i+2];;
    }
    this.push(data);
    done && done();
  })
}
var encodeJPEG = () => {
  return through(function(chunk, enc, done) {
    this.push(Buffer(jpegJs.encode({
      data: chunk,
      width: width,
      height: height
    }, 80).data));
    done && done();
  });
}

var encodeMJPEG = () => {
  return through(function(chunk, enc, done) {
    this.push('--' + boundaryID + '\r\n')
    this.push('Content-Type: image/jpeg\r\n');
    this.push('Content-Length: ' + chunk.length + '\r\n');
    this.push("\r\n");
    this.push(chunk);
    this.push("\r\n");
    done && done();
  });
}

var channelHandler = (type) => {
  return function(req, res) {
    res.writeHead(200, {
      'Content-Type': 'multipart/x-mixed-replace;boundary="' + boundaryID + '"',
      'Connection': 'keep-alive',
      'Expires': 'Fri, 27 May 1977 00:00:00 GMT',
      'Cache-Control': 'no-cache, no-store, max-age=0, must-revalidate',
      'Pragma': 'no-cache'
    });

    var stream = type === 'depth' ? kinect.createDepthStream() : kinect.createVideoStream();

    stream.pipe(reArrangeBuffer(type)).pipe(encodeJPEG()).pipe(encodeMJPEG()).pipe(res);

    req.on('close', function() {
      stream.unpipe();
    });
  }
}

app.use(bodyParser.urlencoded({extended: true}));

app.use(bodyParser.json());

app.get('/', function(req, res) {
  res.sendFile(__dirname + '/index.html');
});

app.get('/bundle.js', browserify(__dirname + '/client.js'));

app.get('/channel_depth', channelHandler('depth'));

app.get('/channel_video', channelHandler('video'));

app.post('/tilt', function(req, res) {
  kinect.setTiltAngle(req.params.angle || req.body.angle || 0);
  res.send({status: 'OK'})
});

http.createServer(app).listen(program.port, program.host, function() {
  console.log('Listen on %s:%s', program.host, program.port);
});
