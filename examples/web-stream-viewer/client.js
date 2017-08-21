var axios = require('axios'),
    channel = window.location.protocol + '//' + window.location.host + '/channel_depth',
    MJPEG = require('./player'),
    slider = document.getElementById('slider1'),
    player = new MJPEG.Player("canvas", channel);
    player.start();

    slider.addEventListener('change',function(e) {
        axios.post('/tilt', {angle: parseInt(e.target.value)});
    }, false)
