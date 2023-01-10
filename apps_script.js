function doGet(e) {
   Logger.log("--- doGet ---");
 
   try {
     // Paste the URL of the Google Sheets starting from https thru /edit
     // For e.g.: https://docs.google.com/..../edit
     var ss = SpreadsheetApp.openByUrl("asd");
     var log_sheet = ss.getSheetByName("sheet");
     var debug_sheet = ss.getSheetByName("debug");
 
     var row = log_sheet.getLastRow() + 1;  // Get last edited row from DataLogger sheet
     if (e == null) {e={}; e.parameters = {tag:"test",value:"-1"};}  // this helps during debugging
 
     var id = e.parameters.id;
     var dur = e.parameters.dur;
     var to = e.parameters.to;
     var upload_time = new Date();
     var start_time = new Date(upload_time.getTime() - dur * 1000);
     var revs = e.parameters.revs;
     var diff = e.parameters.diff;
     var vbat = e.parameters.vbat;
     var rssi = e.parameters.rssi;
     var mac = e.parameters.mac;
 
     if (mac == "11:22:33:44:55:66")
       vbat = vbat * 0.880 + 454;
 
     // Update meta
     debug_sheet.getRange("B1").setValue(upload_time);         // Last modified date
     debug_sheet.getRange("B2").setValue(row);                 // Last modified line
     debug_sheet.getRange("B3").setValue(revs);                // 
     debug_sheet.getRange("B4").setValue(vbat/1000);           // 
 
     if (vbat < 4000 || revs == 0)
       MailApp.sendEmail("example@mail.com", "Battery low", "Battery of the crosstrainer is low. Voltage: " + vbat/1000);
 
     if (revs == 0)
       return ContentService.createTextOutput("Success!");
 
     // Start Populating the data
     log_sheet.getRange("A" + row).setValue(id);         // ID
     log_sheet.getRange("B" + row).setValue(start_time); // Timestamp
     log_sheet.getRange("C" + row).setValue((dur-to)/60);// Duration (in minutes, not including the timeout)
     log_sheet.getRange("D" + row).setValue(revs);       // Number of revolutions
     log_sheet.getRange("E" + row).setValue(diff);       // Difficulty (average)
     log_sheet.getRange("F" + row).setValue("=D"+row+"*E"+row+"*0,00015");     // Total energy
     log_sheet.getRange("G" + row).setValue(vbat/1000);  // V_bat (mV)
     log_sheet.getRange("J" + row).setValue("=D"+row+"/H"+row);   // Distance / rounds
     log_sheet.getRange("K" + row).setValue("=D"+row+"/C"+row);   // Speed
     log_sheet.getRange("L" + row).setValue(rssi);       // Last RSSI
     log_sheet.getRange("M" + row).setValue(mac);        // Last MAC
     log_sheet.getRange("N" + row).setValue(start_time); // Timestamp
     log_sheet.getRange("O" + row).setValue(start_time); // Timestamp
 
     //return ContentService.createTextOutput("Wrote:\n  tag: " + tag + "\n  value: " + value);
     return ContentService.createTextOutput("Success!");
 
   } catch(error) { 
     Logger.log(error);    
     return ContentService.createTextOutput("oops...." + error.message
                                             + "\n" + new Date()
                                             /*+ "\ntag: " + tag +
                                             + "\nvalue: " + value*/);
   }
 }
 