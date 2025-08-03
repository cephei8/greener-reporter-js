const {describe, expect, test, beforeEach, afterEach} = require('@jest/globals');

const {Reporter} = require('greener-reporter');
const {Servermock} = require('greener-servermock');

const fixtureNames = new Servermock().fixtureNames();


describe.each(fixtureNames)('fixture test [%s]', (fixtureName) => {
    const ctx = {
        servermock: undefined,
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
        const port = ctx.servermock.getPort();

        process.env.GREENER_INGRESS_ENDPOINT = "http://localhost:" + port;
        process.env.GREENER_INGRESS_API_KEY = "some-api-token";
    });

    afterEach(() => {
        ctx.servermock.shutdown();
    });

    test('test name', () => {
        const calls = JSON.parse(ctx.calls);
        const responses = JSON.parse(ctx.responses);

        const reporter = new Reporter();

        for (let i = 0; i < calls.calls.length; i++) {
            const c = calls.calls[i];

            if (c.func === 'createSession') {
                const r = responses.createSessionResponse;
                const baggage = c.payload.baggage != null ? JSON.stringify(c.payload.baggage) : null;

                if(c.payload.id != null) {
                    process.env.GREENER_SESSION_ID = c.payload.id;
                }
                if(c.payload.description != null) {
                    process.env.GREENER_SESSION_DESCRIPTION = c.payload.description;
                }
                if(baggage != null) {
                    process.env.GREENER_SESSION_BAGGAGE = baggage;
                }
                if(c.payload.labels != null) {
                    process.env.GREENER_SESSION_LABELS = c.payload.labels;
                }

                try {
                    if (r.status === 'success') {
                        const session = reporter.createSession();
                        expect(session.id).toBe(r.payload.id);
                    } else if (r.status === 'error') {
                        expect(() => {
                            reporter.createSession();
                        }).toThrow(new Error(`GreenerReporterError ${r.payload.code}/${r.payload.ingressCode}: failed session request: ${r.payload.message}`));
                    } else {
                        throw new Error('unknown response status: ' + r.status);
                    }
                } finally {
                    if(c.payload.id != null) {
                        delete process.env.GREENER_SESSION_ID;
                    }
                    if(c.payload.description != null) {
                        delete process.env.GREENER_SESSION_DESCRIPTION;
                    }
                    if(baggage != null) {
                        delete process.env.GREENER_SESSION_BAGGAGE;
                    }
                    if(c.payload.labels != null) {
                        delete process.env.GREENER_SESSION_LABELS;
                    }
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
