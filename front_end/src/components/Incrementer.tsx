import React, { Component } from 'react';
import { WebSocketContextType, WebSocketMessage } from './WebSocketContext';

interface IncrementerProps {
    signal: string;
    socket: WebSocketContextType;
    min?: number;
    max?: number;
    step?: number;
    holdEnabled?: boolean;
    holdIntervalMs?: number;
    boxStyle?: React.CSSProperties;
    buttonStyle?: React.CSSProperties;
}

interface IncrementerState {
    value: number;             // read-back value from server
    requestedValue: number;    // value last sent to server
}

export default class Incrementer extends Component<IncrementerProps, IncrementerState> {
    static defaultProps = {
        min: Number.MIN_SAFE_INTEGER,
        max: Number.MAX_SAFE_INTEGER,
        step: 1,
        holdEnabled: false,
        holdIntervalMs: 100,
    };

    private holdTimer: NodeJS.Timeout | null = null;

    constructor(props: IncrementerProps) {
        super(props);
        this.state = {
            value: 0,
            requestedValue: 0,
        };
    }

    componentDidMount() {
        this.setupSocket();
    }

    componentWillUnmount() {
        this.teardownSocket();
        this.clearHoldTimer();
    }

    setupSocket() {
        const { socket, signal } = this.props;
        if (!socket) return;

        (this as any)._incrementerOnOpen = () => {
            socket.subscribe(signal, this.handleSignalValue);
        };
        socket.onOpen((this as any)._incrementerOnOpen);

        if (socket.isOpen && socket.isOpen()) {
            (this as any)._incrementerOnOpen();
        }
    }

    teardownSocket() {
        const { socket, signal } = this.props;
        if (!socket) return;

        socket.unsubscribe(signal, this.handleSignalValue);

        const onOpen = (this as any)._incrementerOnOpen;
        if (onOpen && socket.removeOnOpen) {
            socket.removeOnOpen(onOpen);
        }
        delete (this as any)._incrementerOnOpen;
    }

    handleSignalValue = (message: WebSocketMessage) => {
        if (message.type === 'signal') {
            const val = message.value;
            if (typeof val === 'number') {
                const clamped = Math.min(
                    this.props.max!,
                    Math.max(this.props.min!, val)
                );
                this.setState({ value: clamped });
            }
        }
    };

    sendValue(value: number) {
        const { socket, signal } = this.props;
        if (socket && socket.isOpen && socket.isOpen()) {
            socket.sendMessage({
                type: 'signal',
                signal,
                value,
            });
            this.setState({ requestedValue: value });
        }
    }

    changeValue(delta: number) {
        const newValue = Math.min(
            this.props.max!,
            Math.max(this.props.min!, this.state.value + delta)
        );
        this.setState({ value: newValue, requestedValue: newValue }, () => {
            this.sendValue(newValue);
        });
    }

    startHold = (delta: number) => {
        const { holdEnabled, holdIntervalMs } = this.props;
        if (!holdEnabled) {
            this.changeValue(delta);
            return;
        }
        // Immediate change once on press
        this.changeValue(delta);

        // Start repeating change every holdIntervalMs
        this.clearHoldTimer();
        this.holdTimer = setInterval(() => {
            this.changeValue(delta);
        }, holdIntervalMs);
    };

    clearHoldTimer = () => {
        if (this.holdTimer) {
            clearInterval(this.holdTimer);
            this.holdTimer = null;
        }
    };

    render() {
        const { boxStyle, buttonStyle, step } = this.props;

        const defaultBoxStyle: React.CSSProperties = {
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            border: '1px solid white',
            borderRadius: 4,
            // color will be dynamic below
            width: 80,
            height: 40,
            userSelect: 'none',
            fontSize: 20,
            fontWeight: 'bold',
            backgroundColor: 'black',
        };

        const defaultButtonStyle: React.CSSProperties = {
            color: 'white',
            backgroundColor: '#333',
            border: 'none',
            padding: '8px 12px',
            fontSize: 20,
            cursor: 'pointer',
            userSelect: 'none',
        };

        // Change color to yellow if requestedValue != read-back value
        const textColor =
            this.state.requestedValue !== this.state.value ? 'yellow' : 'white';

        return (
            <div style={{ display: 'flex', alignItems: 'center', gap: 10 }}>
                <button
                    style={{ ...defaultButtonStyle, ...buttonStyle }}
                    aria-label="-"
                    onMouseDown={() => this.startHold(-(step ?? 1))}
                    onMouseUp={this.clearHoldTimer}
                    onMouseLeave={this.clearHoldTimer}
                    onTouchStart={() => this.startHold(-(step ?? 1))}
                    onTouchEnd={this.clearHoldTimer}
                    onTouchCancel={this.clearHoldTimer}
                >
                    –
                </button>

                <div style={{ ...defaultBoxStyle, ...boxStyle, color: textColor }}>
                    {this.state.value}
                </div>

                <button
                    style={{ ...defaultButtonStyle, ...buttonStyle }}
                    aria-label="+"
                    onMouseDown={() => this.startHold(step ?? 1)}
                    onMouseUp={this.clearHoldTimer}
                    onMouseLeave={this.clearHoldTimer}
                    onTouchStart={() => this.startHold(step ?? 1)}
                    onTouchEnd={this.clearHoldTimer}
                    onTouchCancel={this.clearHoldTimer}
                >
                    +
                </button>
            </div>
        );
    }
}
