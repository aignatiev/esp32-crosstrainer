function doGet(e) {
  try {
    // For easier debugging
    if (e == null) {
      e={};
      e.parameter = {id:"-1", dur:"300", to:"10", revs:"50", diff:"1200", 
          vbat:"4500", rssi:"-90", mac:"11:22:33:44:55:66"};
    }

    // All the necessary variables
    var p = e.parameter;
    var ss = SpreadsheetApp.openByUrl("asd");
    var log_sheet = ss.getSheetByName("sheet");
    var debug_sheet = ss.getSheetByName("debug");
    var row = log_sheet.getLastRow() + 1;
    var upload_time = new Date();
    var start_time = new Date(upload_time.getTime() - p.dur * 1000);

    // ADC compensation (based on mac in case of multiple sensors)
    if (p.mac == "11:22:33:44:55:66")
      p.vbat = p.vbat * 0.880 + 454;

    // Update debug sheet
    debug_sheet.getRange("B1:B4").setValues([[upload_time], [row], [p.revs], [p.vbat/1000]]);

    // Alert the user if battery voltage is low
    if (p.vbat < 4000 || p.revs == 0)
      MailApp.sendEmail("example@email.com", "Battery low", 
          "Battery of the crosstrainer is low. Voltage: " + p.vbat/1000);

    // This is either an error, or only a message for low battery
    if (p.revs == 0)
      return ContentService.createTextOutput("Success!");

    // Start populating the data
    log_sheet.appendRow([p.id, start_time, (p.dur-p.to)/60, p.revs, p.diff, 
        "=D"+row+"*E"+row+"*0,00015", p.vbat/1000, null, null, "=D"+row+"/H"+row,
        "=D"+row+"/C"+row, p.rssi, p.mac, "=B"+row, "=B"+row]);

    return ContentService.createTextOutput("Success!");

  } catch(error) { 
    Logger.log(error);
    return ContentService.createTextOutput("oops...." + error.message + "\n" + new Date());
  }
}
