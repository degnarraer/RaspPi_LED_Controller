import { Component } from 'react';
import { WebSocketContextType, WebSocketMessage } from './WebSocketContext';

interface SignalValueTextBoxProps {
    signal: string;
    socket: WebSocketContextType;
    placeholder?: string;
    className?: string;

    decimalPlaces?: number;  // Optional, defaults to 2
    units?: string;          // Optional, defaults to ''
}

interface SignalValueTextBoxState {
    rawValue: number | null;
    value: string;  // fallback string (placeholder or raw string)
}

export default class SignalValueTextBox extends Component<SignalValueTextBoxProps, SignalValueTextBoxState> {
    constructor(props: SignalValueTextBoxProps) {
        super(props);
        this.state = {
            rawValue: null,
            value: props.placeholder || '—',
        };
    }

    componentDidMount() {
        this.setupSocket();
    }

    componentWillUnmount() {
        this.teardownSocket();
    }

    setupSocket() {
        const { socket, signal } = this.props;
        if (socket?.socket instanceof WebSocket) {
            socket.subscribe(signal, this.handleSignalValue);
        }
    }

    teardownSocket() {
        const { socket, signal } = this.props;
        if (socket?.socket instanceof WebSocket) {
            socket.unsubscribe(signal, this.handleSignalValue);
        }
    }

    handleSignalValue = (message: WebSocketMessage) => {
        if (message.type === 'signal value message') {
            const value = message.value;

            if (typeof value === 'number') {
                this.setState({
                    rawValue: value,
                    value: '',
                });
            } else if (typeof value === 'string') {
                const num = Number(value);
                if (!isNaN(num)) {
                    this.setState({
                        rawValue: num,
                        value: '',
                    });
                } else {
                    this.setState({
                        rawValue: null,
                        value,
                    });
                }
            } else if (value != null) {
                console.error('Invalid non-primitive signal value format:', value);
            } else {
                console.error('Signal value is null or undefined');
            }

        } else if (message.type === 'binary') {
            console.warn('Received unsupported binary data.');
        } else {
            console.warn('Unhandled message type:', message.type);
        }
    };

    render() {
        const { rawValue, value } = this.state;
        const { className, decimalPlaces = 2, units = '' } = this.props;

        let displayValue = value;

        if (rawValue !== null) {
            displayValue = rawValue.toFixed(decimalPlaces) + (units ? ` ${units}` : '');
        }

        return (
            <div
                className={className}
                style={{    color: 'black',
                    padding: '8px 12px',
                    border: '1px solid #ccc',
                    borderRadius: '4px',
                    backgroundColor: '#f9f9f9',
                    fontFamily: 'monospace',
                    fontSize: '1rem',
                    width: '20ch',
                    textAlign: 'center',
                    whiteSpace: 'nowrap',
                    overflow: 'hidden',
                    textOverflow: 'ellipsis'
                }}
            >
                {displayValue}
            </div>
        );
    }
}
