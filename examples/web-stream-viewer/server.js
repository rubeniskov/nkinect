var kinect = require('../../')();
var jpegJs = require('jpeg-js')
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

app.use(bodyParser.urlencoded({
    extended: true
}));

app.use(bodyParser.json());

app.get('/', function(req, res) {
  res.sendFile(__dirname + '/index.html');
});

app.get('/bundle.js', browserify(__dirname + '/client.js'));

app.get('/channel_depth', function(req, res) {

  res.writeHead(200, {
    'Content-Type': 'multipart/x-mixed-replace;boundary="' + boundaryID + '"',
    'Connection': 'keep-alive',
    'Expires': 'Fri, 27 May 1977 00:00:00 GMT',
    'Cache-Control': 'no-cache, no-store, max-age=0, must-revalidate',
    'Pragma': 'no-cache'
  });

  kinect.startDepth(function(buf) {
    var data = new Buffer(width * height * 4);

    for(var i=0; i < width * height; i++){

        var depth = (buf[2*i]+buf[2*i+1]*255)/5;
        data[4*i] = depth;
        data[4*i+1] = depth;
        data[4*i+2] = depth;
        data[4*i+3] = 255;
    }

    res.write('--' + boundaryID + '\r\n')
    res.write('Content-Type: image/jpeg\r\n');
    res.write('Content-Length: ' + data.length + '\r\n');
    res.write("\r\n");
    res.write(Buffer(jpegJs.encode({
        data: data,
        width: width,
        height: height
    }, 80).data), 'binary');
    res.write("\r\n");


  });

  req.on('close', function() {
    kinect.pause();
  });
});

app.post('/tilt', function(req, res) {
    kinect.titlAngle(req.params.angle||req.body.angle||0);
    res.send({status: 'OK'})
});

app.get('/channel_video', function(req, res) {

  res.writeHead(200, {
    'Content-Type': 'multipart/x-mixed-replace;boundary="' + boundaryID + '"',
    'Connection': 'keep-alive',
    'Expires': 'Fri, 27 May 1977 00:00:00 GMT',
    'Cache-Control': 'no-cache, no-store, max-age=0, must-revalidate',
    'Pragma': 'no-cache'
  });

  kinect.startVideo(function(buf) {
    // var array = ndarray(buf, [width, height, 3], [3, 3 * width, 1], 0);
    var data = new Buffer(width * height * 4);

    for(var i=0; i < width * height; i++){
        data[4*i]   = buf[3*i];
        data[4*i+1] = buf[3*i+1];
        data[4*i+2] = buf[3*i+2];
        data[4*i+3] = 255;
    }

    // ops.assign(
    //   ndarray(data,
    //     [width, height, 3],
    //     [4, 4 * width, 1]), array);
    // ops.assigns(
    //   ndarray(data,
    //     [width * height],
    //     [4], 3), 255);

    res.write('--' + boundaryID + '\r\n')
    res.write('Content-Type: image/jpeg\r\n');
    res.write('Content-Length: ' + data.length + '\r\n');
    res.write("\r\n");
    res.write(Buffer(jpegJs.encode({
        data: data,
        width: width,
        height: height
    }, 80).data), 'binary');
    res.write("\r\n");


  });

  req.on('close', function() {
    kinect.pause();
  });
});

kinect.resume();

http.createServer(app).listen(program.port, program.host, function() {
  console.log('Listen on %s:%s', program.host, program.port);
});
