TCPClient serverClient;
byte server[] = { 192, 168, 100, 100 };
int port = 8888;

const char* serviceUuid = "58F75BE1-6DF6-4273-9627-CA053E89771B";
const char* sensorMode  = "58F75BE2-6DF6-4273-9627-CA053E89771B";
const size_t SCAN_RESULT_COUNT = 10;
BleScanResult scanResults[SCAN_RESULT_COUNT];
BlePeerDevice peer;
BleCharacteristic peerModeCharacteristic;

String readServer(char *bufferStr);
void reportDone(size_t scanCount, int id);

// setup() runs once, when the device is first turned on.
void setup() {
  // Put initialization like pinMode and begin functions here.
  Serial.begin();

  BLE.setScanPhy(BlePhy::BLE_PHYS_AUTO);
  BLE.selectAntenna(BleAntennaType::EXTERNAL);

  delay(5s);
}

// loop() runs over and over again, as quickly as it can execute.
void loop() {
  //BLE devices scan
  size_t count = BLE.scanWithFilter(BleScanFilter().serviceUUID(serviceUuid), scanResults, SCAN_RESULT_COUNT);
  Serial.printlnf("Found %d device(s) exposing service %s", count, (const char*)serviceUuid);

  String bleMAC[count];
  char *bufferStr1 = (char *) malloc(400);
  JSONBufferWriter writer(bufferStr1, 399);
  writer.beginArray();
  writer.value(1);
  for (uint8_t ii = 0; ii < count; ii++)
  {
    writer.value(scanResults[ii].address().toString());
  }
  writer.endArray();
  writer.buffer()[std::min(writer.bufferSize(), writer.dataSize())] = 0;

  // Get data from server
  Serial.println("request data from server");
  String inData = readServer(bufferStr1);
  free(bufferStr1);
  Serial.print("received: ");
  Serial.println(inData);

  // Parse & process received data
  JSONValue outerObj = JSONValue::parseCopy(inData);
  JSONObjectIterator iter(outerObj);
  while (iter.next())
  {
    String name = (const char *) iter.name();
    String value = (const char *) iter.value().toString();
    int id = name.toInt();
    int mode = value.toInt();

    // Connect to specified BLE scan result
    peer = BLE.connect(scanResults[id].address());
    waitFor(peer.connected, 20000);
    if (peer.connected())
    {
      peer.getCharacteristicByUUID(peerModeCharacteristic, sensorMode);
      if (mode == 1)
      {
        uint8_t value = 0xff;
        peerModeCharacteristic.setValue(value);
        reportDone(count, id);
      }
      else if (mode == 2)
      {
        uint8_t value = 0x00;
        peerModeCharacteristic.setValue(value);
        reportDone(count, id);
      }
      delay(100);
      peer.disconnect();
    }
  }

  delay(100);
}


String readServer(char *bufferStr)
{
  // Send request to TCP server
  serverClient.connect(server, port);
  serverClient.print(bufferStr);
  Serial.print("sent: ");
  Serial.println(bufferStr);

  // Read incoming data from Server
  String inData = "";
  waitFor(serverClient.available, 5000);
  while (serverClient.available() > 0)
  {
    char c = serverClient.read();
    inData += c;
  }
  serverClient.stop();
  return inData;
}

void reportDone(size_t scanCount, int id)
{
  // Construct data to be sent
  char *bufferStr2 = (char *) malloc(400);
  JSONBufferWriter writer(bufferStr2, 399);
  writer.beginArray();
  writer.value(2);
  writer.value(id);
  for (uint8_t ii = 0; ii < scanCount; ii++)
  {
    writer.value(scanResults[ii].address().toString());
  }
  writer.endArray();
  writer.buffer()[std::min(writer.bufferSize(), writer.dataSize())] = 0;

  // Send request to TCP server
  serverClient.connect(server, port);
  serverClient.print(bufferStr2);
  Serial.print("report done to server: ");
  Serial.println(bufferStr2);

  // Read incoming data from Server
  String inData = "";
  waitFor(serverClient.available, 5000);
  serverClient.stop();
  free(bufferStr2);
  return;
}