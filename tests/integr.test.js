const {describe, expect, test, beforeEach, afterEach} = require('@jest/globals');

const {Reporter} = require('greener-reporter');
const {Servermock} = require('greener-servermock');

const fixtureNames = new Servermock().fixtureNames();


describe.each(fixtureNames)('fixture test [%s]', (fixtureName) => {
    const ctx = {
        servermock: undefined,
        port: undefined,
        serverAddress: undefined,
        apiToken: undefined,
        calls: undefined,
        responses: undefined,
    };

    beforeEach(() => {
        ctx.servermock = new Servermock();

        ctx.calls = ctx.servermock.fixtureCalls(fixtureName);
        ctx.responses = ctx.servermock.fixtureResponses(fixtureName);

        ctx.servermock.serve(ctx.responses);
        ctx.port = ctx.servermock.getPort();
    });

    afterEach(() => {
        ctx.servermock.shutdown();
    });

    test('test name', () => {
        const calls = JSON.parse(ctx.calls);
        const responses = JSON.parse(ctx.responses);

        const reporter = new Reporter("http://localhost:" + ctx.port, "some-api-key");

        for (let i = 0; i < calls.calls.length; i++) {
            const c = calls.calls[i];

            if (c.func === 'createSession') {
                const r = responses.createSessionResponse;
                const baggage = c.payload.baggage != null ? JSON.stringify(c.payload.baggage) : null;

                if (r.status === 'success') {
                    const session = reporter.createSession(c.payload.id, c.payload.description, baggage, c.payload.labels);
                    expect(session.id).toBe(r.payload.id);
                } else if (r.status === 'error') {
                    expect(() => {
                        reporter.createSession(c.payload.id, c.payload.description, baggage, c.payload.labels);
                    }).toThrow(new Error(`GreenerReporterError ${r.payload.code}/${r.payload.ingressCode}: failed session request: ${r.payload.message}`));
                } else {
                    throw new Error('unknown response status: ' + r.status);
                }

            } else if (c.func === 'report') {
                const r = responses.reportResponse;
                if (r.status === 'success') {
                    for (let p of c.payload.testcases) {
                        reporter.createTestcase(
                            p.sessionId,
                            p.testcaseName,
                            p.testcaseClassname,
                            p.testcaseFile,
                            p.testsuite,
                            p.status,
                            null,
                            null
                        );
                    }
                } else if (r.status === 'error') {
                    expect(() => {
                        for (let p of c.payload.testcases) {
                            reporter.createTestcase(
                                p.sessionId,
                                p.testcaseName,
                                p.testcaseClassname,
                                p.testcaseFile,
                                p.testsuite,
                                p.status,
                                null,
                                null
                            );
                        }
                    }).toThrow(new Error(`GreenerReporterError ${r.payload.code}/${r.payload.ingressCode}: ${r.payload.message}`));
                } else {
                    throw new Error('unknown response status: ' + r.status);
                }

            } else {
                throw new Error('unknown call func: ' + c.func);
            }
        }

        reporter.shutdown();

        ctx.servermock.assert(ctx.calls);
    });
});
