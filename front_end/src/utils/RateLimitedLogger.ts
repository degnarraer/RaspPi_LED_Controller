export class RateLimitedLogger {
    private rateLimitMs: number;
    private logMap: Map<string, { lastLogTime: number; occurrences: number }>;

    constructor(rateLimitMs: number) {
        this.rateLimitMs = rateLimitMs;
        this.logMap = new Map();
    }

    log(key: string, message: string) {
        const currentTime = Date.now();
        const entry = this.logMap.get(key) || { lastLogTime: 0, occurrences: 0 };

        entry.occurrences++;

        if (currentTime - entry.lastLogTime >= this.rateLimitMs) {
            console.log(`${message} (Occurred ${entry.occurrences} times)`);
            entry.lastLogTime = currentTime;
            entry.occurrences = 0;
        }

        this.logMap.set(key, entry);
    }
}