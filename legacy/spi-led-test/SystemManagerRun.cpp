// void SystemManager::run() {
    /* Stress test
    buttonMng.updateAll();
    now = millis();
    
    if (xQueueReceive(dataQueue, (void *)&receivedData, sizeof(QueuePacket)) == true) {
        setSystemState(receivedData.type);
        
        if (receivedData.type == EventType::DATA) {
            newData = true;
            
            // KÖR STRESSTEST EN GÅNG
            static bool testRun = false;
            if (!testRun) {
                testRun = true;
                Serial.println(">>> Data mottagen - kör stresstest <<<");
                matrix.stressTest();
            }
            
            if (receivedData.direction[0].count > 0) {
                Serial.print("Next departure: "); 
                Serial.print(receivedData.direction[0].departures[0].minutes); 
                Serial.println(" min");
            }
        }
    }
    */