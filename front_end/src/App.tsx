import { useState } from 'react';
import { Button, Drawer, Menu } from 'antd';
import { WebSocketProvider } from './components/WebSocketContext';
import LiveBarChart from './components/LiveBarChart';
import WebSocketWrapper from './components/WebSocketWrapper';

function App() {
  const [visible, setVisible] = useState(false);
  const [screen, setScreen] = useState('home');

  const openDrawer = () => setVisible(true);
  const closeDrawer = () => setVisible(false);

  const labels = [
    '16 Hz', '20 Hz', '25 Hz', '31.5 Hz', '40 Hz', '50 Hz', '63 Hz', '80 Hz', '100 Hz', '125 Hz', 
    '160 Hz', '200 Hz', '250 Hz', '315 Hz', '400 Hz', '500 Hz', '630 Hz', '800 Hz', '1000 Hz', '1250 Hz', 
    '1600 Hz', '2000 Hz', '2500 Hz', '3150 Hz', '4000 Hz', '5000 Hz', '6300 Hz', '8000 Hz', '10000 Hz', 
    '12500 Hz', '16000 Hz', '20000 Hz'
  ];
  
  const initialData = [
    0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9, 2.0, 
    2.1, 2.2, 2.3, 2.4, 2.5, 2.6, 2.7, 2.8, 2.9, 3.0, 3.1, 3.2
  ];

  function HomeScreen() {
    return <h1>Home Screen</h1>;
}

function BandsLeftChannelScreen() {
  return(
      <div><h1>Left Bands Channel</h1>
        <div>
          <WebSocketWrapper>
            <LiveBarChart labels={labels} initialData={initialData} signal="FFT Bands Left Channel"/>
          </WebSocketWrapper>
        </div>
      </div>
  );
}


function BandsRightChannelScreen() {
  return(
    <div><h1>Right Bands Channel</h1>
        <div>
          <WebSocketWrapper>
            <LiveBarChart labels={labels} initialData={initialData} signal="FFT Bands Right Channel"/>
          </WebSocketWrapper>
        </div>
    </div>
  );
}

  return (
    <WebSocketProvider url="ws://ltop.local:8080">
      <div>
        <Button type="primary" onClick={openDrawer}>
          Open Menu
        </Button>
        <Drawer title="Navigation" placement="left" onClose={closeDrawer} visible={visible}>
          <Menu>
            <Menu.Item key="1" onClick={() => { setScreen('home'); closeDrawer(); }}>Home</Menu.Item>
            <Menu.Item key="2" onClick={() => { setScreen('bands right channel'); closeDrawer(); }}>Bands Right Channel</Menu.Item>
            <Menu.Item key="3" onClick={() => { setScreen('bands left channel'); closeDrawer(); }}>Bands Left Channel</Menu.Item>
          </Menu>
        </Drawer>
        {screen === 'home' && <HomeScreen />}
        {screen === 'bands right channel' && <BandsRightChannelScreen />}
        {screen === 'bands left channel' && <BandsLeftChannelScreen />}
      </div>
    </WebSocketProvider>
  );
}

export default App;
