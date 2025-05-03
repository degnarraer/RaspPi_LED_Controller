
export class RateLimitedLogger {
    private lastLogTime: number;
    private rateLimitMs: number;
    private occurrences: number;

    constructor(rateLimitMs: number) {
        this.lastLogTime = 0;
        this.rateLimitMs = rateLimitMs;
        this.occurrences = 0;
    }

    log(message: string) {
        const currentTime = Date.now();
        this.occurrences++;

        if (currentTime - this.lastLogTime >= this.rateLimitMs) {
            console.log(`${message} (Occurred ${this.occurrences} times)`);
            this.lastLogTime = currentTime;
            this.occurrences = 0;
        }
    }
}
