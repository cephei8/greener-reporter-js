const _reporter = require('bindings')('greener_reporter_addon');

class Reporter {
  constructor() {
    this._handle = new _reporter.Reporter(
      process.env.GREENER_INGRESS_ENDPOINT,
      process.env.GREENER_INGRESS_API_KEY
    );
  }

  createSession() {
    let id = process.env.GREENER_SESSION_ID;
    let description = process.env.GREENER_SESSION_DESCRIPTION;
    let baggage = process.env.GREENER_SESSION_BAGGAGE;
    let labels = process.env.GREENER_SESSION_LABELS;

    if (id == undefined) {
      id = null;
    }
    if (description == undefined) {
      description = null;
    }
    if (baggage == undefined) {
      baggage = null;
    }
    if (labels == undefined) {
      labels = null;
    }

    return this._handle.createSession(id, description, baggage, labels);
  }

  createTestcase(...args) {
    this._handle.createTestcase(...args);
  }

  shutdown(...args) {
    this._handle.shutdown(...args);
  }
}

module.exports = { Reporter };
