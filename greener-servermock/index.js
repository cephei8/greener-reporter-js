const path = require('path');
const platform = process.platform;
const arch = process.arch;

const addon = require(path.join(__dirname, 'prebuilds', `${platform}-${arch}`, 'greener-servermock.node'));

module.exports = addon;
