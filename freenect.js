// var addon = require('bindings')('freenect');
var addon = require('./build/Release/freenect.node');

module.exports = function(){
    return addon.Freenect();
}
