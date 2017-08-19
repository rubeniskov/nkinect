var addon = require('bindings')('nkinect');

module.exports = function(){
    return addon.NKinect();
}
