import { Component } from 'react';
import { WebSocketContextType, WebSocketMessage } from './WebSocketContext';

interface SignalValueTextBoxProps {
    signal: string;
    socket: WebSocketContextType;
    placeholder?: string;
    className?: string;
}

interface SignalValueTextBoxState {
    value: string;
}

export default class SignalValueTextBox extends Component<SignalValueTextBoxProps, SignalValueTextBoxState> {
    constructor(props: SignalValueTextBoxProps) {
        super(props);
        this.state = {
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

            if (typeof value === 'string' || typeof value === 'number') {
                this.setState({ value: String(value) });
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
        const { value } = this.state;
        const { className } = this.props;

        return (
            <div
                className={className}
                style={{
                    color: 'black',
                    padding: '8px 12px',
                    border: '1px solid #ccc',
                    borderRadius: '4px',
                    backgroundColor: '#f9f9f9',
                    fontFamily: 'monospace',
                    fontSize: '1rem',
                    minWidth: '80px',
                    textAlign: 'center',
                }}
            >
                {value}
            </div>
        );
    }
}
