import { FC } from 'react';
import HorizontalGauge from './HorizontalGauge';
import { WebSocketContextType } from './WebSocketContext';

interface GaugeProps {
  signalName: string;
  socket: WebSocketContextType;
}

const LEDBoardTempGauge: FC<GaugeProps> = ({ signalName, socket }) => {
  return (
    <div style={{ width: '100%', height: '100%' }}>
      <HorizontalGauge
        min={0}
        max={120}
        signal={signalName}
        socket={socket}
        zones={[
          { from: 0, to: 60, color: 'green' },
          { from: 60, to: 90, color: 'yellow' },
          { from: 90, to: 120, color: 'red' },
        ]}
        tickMarks={[0, 30, 60, 90, 120]}
      />
    </div>
  );
};

export default LEDBoardTempGauge;
