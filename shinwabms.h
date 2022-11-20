#include "esphome.h"

class ShinwaBMSuartComponent : public Component, public UARTDevice {

public:
	ShinwaBMSuartComponent(UARTComponent *parent) : UARTDevice(parent) {}

	Sensor *batteryVoltage = new Sensor();
	Sensor *batteryCurrent = new Sensor();
	//Sensor *tempSensor[5] = {new Sensor(),new Sensor(),new Sensor(),new Sensor(),new Sensor()};
	Sensor *tempSensorMax = new Sensor();
	Sensor *tempSensorAvg = new Sensor();
	Sensor *tempSensorMin = new Sensor();
	/*Sensor *batteryCell[16] = {
		new Sensor(),new Sensor(),new Sensor(),new Sensor(),
		new Sensor(),new Sensor(),new Sensor(),new Sensor(),
		new Sensor(),new Sensor(),new Sensor(),new Sensor(),
		new Sensor(),new Sensor(),new Sensor(),new Sensor()
	};*/
	Sensor *batteryCellMax = new Sensor();
	Sensor *batteryCellMin = new Sensor();
	Sensor *batteryCellAvg = new Sensor();
	Sensor *batteryCapacity = new Sensor();

	float cellVoltage[16];
	//TODO flags
	float current;
	int soc;
	float capacity;
	float temperature[5];
	int alarm[5];
	int cyclesCount;
	float voltage;
	float soh;
	int reserved;

	char crc(char *buf, byte len)
	{
		byte i, chk = 0;
		int sum = 0;
		for (i = 0; i < len; i++)
		{
			chk ^= buf[i];
			sum += buf[i];
		}
		return (byte)((chk ^ sum) & 0xFF);
	}

	void send(byte addr, byte cmd, char *data)
	{
		static char buf[10];
		byte size = 0;

		buf[size++] = 0x7E;
		buf[size++] = addr;
		buf[size++] = cmd;
		buf[size++] = 0x00; //data length
		//TODO: add data
/*
	if($data === null){
		$pkt .= chr(0);
	}else{
		$pkt .= chr(strlen($data));
		$pkt .= $data;
	}
*/
		byte checksum = crc(buf, size);
		buf[size++] = checksum;
		buf[size++] = 0x0D;
		ESP_LOGD("custom", "Send pkt %d %d checksum: %d", addr, cmd, checksum);
		write_array((const uint8_t*)buf, size);
	}

	void setup() override {
		ESP_LOGD("custom", "setup()");
		byte limit = 20;
		while (!available()) {
			send(0x00, 0x01, NULL);
			if(--limit <= 0){
				ESP_LOGD("custom", "hear nothing, break");
				break;
			}
			//usleep(1000); TODO
		}
	}

	int readblock()
	{
		byte readed = 0;
		byte subcmd = readlog(); readed++;
		switch (subcmd)
		{
			case 0x01:{ //cell voltage
				byte cellcount = readlog(); readed++;
				if(cellcount>16){
					ESP_LOGD("custom", "Cell count too high: %d", cellcount);
					return 100;
				}
				

				const byte f_bal = 0x80;
				const byte f_ov = 0x40;
				const byte f_uv = 0x20;
				float cellmin = 10.0;
				float cellmax = 0.0;
				float cellavg = 0.0;

				for(byte i=0; i<cellcount; i++){
					byte val = readlog(); readed++;
					if(val & f_bal){
						val &= (byte)(~f_bal);
						//TODO set flag
					}
					if(val & f_ov){
						val &= (byte)(~f_ov);
						//TODO set flag
					}
					if(val & f_uv){
						val &= (byte)(~f_uv);
						//TODO set flag
					}

					cellVoltage[i] = (val*256 + readlog())/1000.0; 
					readed++;
					ESP_LOGD("custom", "Cell %d  %f V", i, cellVoltage[i]);
					//batteryCell[i]->publish_state(cellVoltage[i]);

					if(cellVoltage[i] > cellmax)cellmax = cellVoltage[i];
					if(cellVoltage[i] < cellmin)cellmin = cellVoltage[i];
					cellavg += cellVoltage[i];
				}

				batteryCellMin->publish_state(cellmin);
				batteryCellMax->publish_state(cellmax);
				batteryCellAvg->publish_state(cellavg/cellcount);
				break;
			}
			case 0x02:{ //current
				byte shunts = readlog(); readed++;
				for(byte i=0; i<shunts; i++){
					current = (30000 - (readlog()*256 + readlog())) / 100.0;
					readed+=2;
					ESP_LOGD("custom", "Current %d  %f A", i, current);
					batteryCurrent->publish_state(current);
				}
				break;
			}
			case 0x03:{ //SOC (must be 0-100)
				byte socs = readlog(); readed++;
				for(byte i=0; i<socs; i++){
					soc = ((readlog()*256 + readlog())) / 100;
					readed+=2;
					ESP_LOGD("custom", "SoC %d  %d %%", i, soc);
				}
				break;
			}
			case 0x04:{ //Capacity (1-600Ah, 0.01Ah unit)
				byte caps = readlog(); readed++;
				for(byte i=0; i<caps; i++){
					capacity = ((readlog()*256 + readlog())) / 100.0;
					readed+=2;
					batteryCapacity->publish_state(capacity);
					ESP_LOGD("custom", "Capacity %d  %f Ah", i, capacity);
				}
				break;
			}
			case 0x05:{ //Tempearature (offset -50V)
				byte temps = readlog(); readed++;
				float tempmax = 0.0, tempmin = 255.0, avgtemp = 0;
				for(byte i=0; i<temps; i++){
					temperature[i] = 50-(readlog()*256 + readlog())/10.0;
					readed+=2;
					ESP_LOGD("custom", "Temp %d  %f C", i, temperature[i]);
					if(temperature[i] < 0 || temperature[i] > 40)continue;
					if(i < 4){
						//TODO: temp4 remove flags?
						//tempSensor[i]->publish_state(temperature[i]);
						if(temperature[i] > tempmax)tempmax = temperature[i];
						if(temperature[i] < tempmin)tempmin = temperature[i];
						avgtemp += temperature[i];
					}
				}
				//128 129
				//-78 -79

				tempSensorMax->publish_state(tempmax);
				tempSensorAvg->publish_state(avgtemp / (temps-1));
				tempSensorMin->publish_state(tempmin);
				break;
			}
			case 0x06:{ //Alarm
				byte alarms = readlog(); readed++;
				for(byte i=0; i<alarms; i++){
					alarm[i] = (readlog()*256 + readlog());
					readed+=2;
					ESP_LOGD("custom", "Alarm %d %d", i, alarm[i]);
				}
				break;
			}
			case 0x07:{ //Cycle count
				byte cycles = readlog(); readed++;
				for(byte i=0; i<cycles; i++){
					cyclesCount = (readlog()*256 + readlog());
					readed+=2;
					ESP_LOGD("custom", "Cycles %d %d", i, cyclesCount);
				}
				break;
			}
			case 0x08:{ //Battery voltage (10mV unit)
				byte batts = readlog(); readed++;
				for(byte i=0; i<batts; i++){
					voltage = (readlog()*256 + readlog())/100.0;
					readed+=2;
					ESP_LOGD("custom", "Battery %d %f", i, voltage);
					batteryVoltage->publish_state(voltage);
				}
				break;
			}
			case 0x09:{ //SOH (must be 0-100)
				byte sohs = readlog(); readed++;
				for(byte i=0; i<sohs; i++){
					soh = (readlog()*256 + readlog())/100.0;
					readed+=2;
					ESP_LOGD("custom", "SOH %d %f", i, soh);
				}
				break;
			}
			case 0x0A:{ //Reserved
				byte reserv = readlog(); readed++;
				for(byte i=0; i<reserv; i++){
					reserved = (readlog()*256 + readlog());
					readed+=2;
					ESP_LOGD("custom", "Reserved %d %d", i, reserved);
				}
				break;
			}
			case 0x20:{ //NOOP
				ESP_LOGD("custom", "NOOP");
				break;
			}
			case 0x0D:{ //EOF
				ESP_LOGD("custom", "End of packet");
				break;
			}
			default:{ //some shit happens
				ESP_LOGD("custom", "Dummy read (%d)", subcmd);
				while(available()){
					byte dummy = readlog(); readed++;
					if(dummy == 0x0D || dummy == 0xFF)break;
				}
			}
		}

		return readed;
	}

	int recv()
	{
		//const int max_line_length = 80;
		//static char buffer[max_line_length];

		//header magic
		byte magic = readlog();
		if(magic != 0x7E){
			ESP_LOGD("custom", "Invalid magic: %d", magic);
			return -1;
		}

		//device address
		byte addr = readlog();
		if(addr == 0x00){

			//command id 
			byte cmd = readlog();
			if(cmd == 0x01){
				byte datalen = readlog();
				byte readed = 0;
				while(readed < datalen){
					ESP_LOGD("custom", "Readed %d of %d", readed, datalen);
					readed += readblock();
				}
				return 1;
			}else{
				ESP_LOGD("custom", "Unknown command: %d", cmd);
			}

		}else{
			ESP_LOGD("custom", "Unknown address: %d", addr);
		}
		return 0;
	}

	byte readlog()
	{
		byte tmp = read();
		//ESP_LOGD("custom", " %02x", tmp);
		return tmp;
	}

	void loop() override {
		static int last_update;
		if((last_update + (30 * 1000)) < millis()){
			last_update = millis();

			ESP_LOGD("custom", "loop()");
			while(available())read();
			send(0x00, 0x01, NULL);
			if(recv()){}
		}
	}
};
