import { useState, useContext, useEffect } from 'react';
import { Button, Drawer, Menu } from 'antd';
import { WebSocketContext } from './components/WebSocketContext';
import { renderScreen, ScreenType, SCREENS } from './Screens.tsx';

import {
  MenuOutlined,
  SettingOutlined,
  HomeOutlined,
  BarChartOutlined,
  TableOutlined,
  HeatMapOutlined,
  LineChartOutlined,
  BulbOutlined,
  PlayCircleOutlined,
  ArrowLeftOutlined,
} from '@ant-design/icons';

interface MenuItem {
  key: string;
  targetScreen?: ScreenType;
  targetMenu?: 'main' | 'settings' | 'animations' | 'heatmap';
  label: string;
  icon: React.ReactNode;
}

const MENU_STRUCTURE = {
  main: {
    label: 'Navigation',
    items: [
      { key: 'home', targetScreen: SCREENS.HOME, label: 'Home', icon: <HomeOutlined style={{ fontSize: '40px' }} /> },
      { key: 'settings', targetMenu: 'settings', label: 'Settings', icon: <SettingOutlined style={{ fontSize: '40px' }} /> },
      { key: 'animations', targetMenu: 'animations', label: 'Animations', icon: <PlayCircleOutlined style={{ fontSize: '40px' }} /> },
    ],
  },
  settings: {
    label: 'Settings',
    items: [
      { key: 'brightness', targetScreen: SCREENS.SETTING_BRIGHTNESS, label: 'Brightness', icon: <BulbOutlined style={{ fontSize: '40px' }} /> },
      { key: 'back', targetMenu: 'main', label: 'Back', icon: <ArrowLeftOutlined style={{ fontSize: '40px' }} /> },
    ],
  },
  animations: {
    label: 'Animations',
    items: [
      { key: 'tower screen', targetScreen: SCREENS.TOWER_SCREEN, label: 'Tower Screen', icon: <TableOutlined style={{ fontSize: '40px' }} /> },
      { key: 'horizontal stereo spectrum', targetScreen: SCREENS.HORIZONTAL_STEREO_SPECTRUM, label: 'Stereo Spectrum', icon: <BarChartOutlined style={{ fontSize: '40px' }} /> },
      { key: 'vertical stereo spectrum', targetScreen: SCREENS.VERTICAL_STEREO_SPECTRUM, label: 'Vertical Stereo Spectrum', icon: <BarChartOutlined style={{ fontSize: '40px', transform: 'scaleX(-1) rotate(-90deg)' }} /> },
      { key: 'wave screen', targetScreen: SCREENS.WAVE_SCREEN, label: 'Wave Screen', icon: <LineChartOutlined style={{ fontSize: '40px' }} /> },
      { key: 'heat map', targetMenu: 'heatmap', label: 'Heat Map', icon: <HeatMapOutlined style={{ fontSize: '40px' }} /> },
      { key: 'back', targetMenu: 'main', label: 'Back', icon: <ArrowLeftOutlined style={{ fontSize: '40px' }} /> },
    ],
  },
  heatmap: {
    label: 'Heat Map',
    items: [
      { key: 'scrolling heat map screen', targetScreen: SCREENS.SCROLLING_HEAT_MAP, label: 'Normal', icon: <HeatMapOutlined style={{ fontSize: '40px' }} /> },
      { key: 'scrolling heat map rainbow screen', targetScreen: SCREENS.SCROLLING_HEAT_MAP_RAINBOW, label: 'Rainbow', icon: <HeatMapOutlined style={{ fontSize: '40px' }} /> },
      { key: 'back', targetMenu: 'animations', label: 'Back', icon: <ArrowLeftOutlined style={{ fontSize: '40px' }} /> },
    ],
  },
} as const;


function App() {
  const socket = useContext(WebSocketContext);
  const [visible, setVisible] = useState(false);
  const [screen, setScreen] = useState<ScreenType>(SCREENS.HORIZONTAL_STEREO_SPECTRUM);
  const menuKeys = Object.keys(MENU_STRUCTURE) as Array<keyof typeof MENU_STRUCTURE>;
  const [menuStack, setMenuStack] = useState<(keyof typeof MENU_STRUCTURE)[]>([menuKeys[0]]);
  const currentMenuKey = menuStack[menuStack.length - 1];
  const currentMenu = MENU_STRUCTURE[currentMenuKey];

  const openDrawer = () => setVisible(true);
  const closeDrawer = () => setVisible(false);

  useEffect(() => {
    const handleEsc = (e: KeyboardEvent) => {
      if (e.key === 'Escape') {
        closeDrawer();
      }
    };
    window.addEventListener('keydown', handleEsc);
    return () => {
      window.removeEventListener('keydown', handleEsc);
    };
  }, []);

  const handleMenuClick = (item: MenuItem) => {
    if ('targetScreen' in item && item.targetScreen) {
      setScreen(item.targetScreen);
      setVisible(false);
      setMenuStack(['main']);
    } else if ('targetMenu' in item && item.targetMenu) {
      if (item.label.toLowerCase() === 'back') {
        setMenuStack(prev => (prev.length > 1 ? prev.slice(0, -1) : prev));
      } else {
        setMenuStack(prev => [...prev, item.targetMenu as keyof typeof MENU_STRUCTURE]);
      }
    }
  };

  return (
    <div style={{ width: '100%', height: '100%', position: 'absolute', top: 0, left: 0 }}>
      <Drawer
        title={currentMenu.label}
        placement="right"
        onClose={() => {
          setVisible(false);
          setMenuStack(['main']);
        }}
        open={visible}
      >
        <Menu style={{ zIndex: 20001 }}>
          {currentMenu.items.map(item => (
            <Menu.Item key={item.key} icon={item.icon} onClick={() => handleMenuClick(item)}>
              {item.label}
            </Menu.Item>
          ))}
        </Menu>
      </Drawer>

      {renderScreen({ socket, screen })}

      <Button
        type="primary"
        onClick={openDrawer}
        style={{
          position: 'fixed',
          top: '10px',
          right: '10px',
          zIndex: 10000,
          opacity: 0.25,
        }}
        icon={
          <span style={{ padding: '4px' }}>
            <MenuOutlined style={{ fontSize: '20px' }} />
          </span>
        }
      >
        Menu
      </Button>
    </div>
  );
}

export default App;

