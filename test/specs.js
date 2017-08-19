import pkg from "../package.json";
import util from "util";
import chai from "chai";
import chaiThings from "chai-things";
import chaiAsPromised from "chai-as-promised";
import sinon from "sinon";
import sinonChai from "sinon-chai";

// process.env.DYLD_INSERT_LIBRARIES="/usr/local/lib/fakenect/libfreenect.dylib"
// process.env.DYLD_FORCE_FLAT_NAMESPACE="y";
global.sinon = sinon;
global.expect = chai
    .use(chaiThings)
    .use(chaiAsPromised)
    .use(sinonChai)
    .expect;


describe(util.format('%s@%s', pkg.name, pkg.version), () => {
    describe('#Core', () => {
        require('./spec-nkinect-initialization');
    });
});
