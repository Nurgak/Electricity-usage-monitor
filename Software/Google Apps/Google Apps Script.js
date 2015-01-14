// Electricity Usage Monitor input function saves data to the spreadsheet via a GET request
function doGet(e)
{
  var SPREADSHEET_ID = "***";
  
  var hourOffset = -1;
  
  // Sheet names
  var sheet = {"day": "Day", "month": "Month"};
  
  // Column IDs
  var column = {"date": 1, "power": 2, "cost": 3, "data": 4};
  
  var data = e.parameter;
  //var data = {"time": "1422748800", "power": "42"}; // Dummy data for debugging
  
  if(!data.time || !data.power)
  {
    return ContentService.createTextOutput("Missing data");
  }
  
  // For debug this holds the last submitted data
  Logger.log(data.time);
  Logger.log(data.power);
  
  // Data validation
  if(!data.time.match(/^\d+$/) || !data.power.match(/^\d+$/))
  {
    return ContentService.createTextOutput("Erroneous data");
  }
  
  // Offset time
  data.time = parseInt(data.time) + 3600 * hourOffset;
  
  // Javascript works in milliseconds to multiply by 1000
  var d = new Date(data.time * 1000);
  var d_y = d.getFullYear();
  var d_m = d.getMonth() + 1; // because January = 0
  var d_d = d.getDate();
  
  // Convert the Unix time to YYYY-MM-DD format
  data.date = d_y + "-" + (d_m < 10 ? "0" + d_m : d_m) + "-" + (d_d < 10 ? "0" + d_d : d_d);
  
  Logger.log(data.date);
  Logger.log(d.getHours());
  
  // Sheet object, must call by ID or it won't know which sheet to update
  var spreadsheet = SpreadsheetApp.openById(SPREADSHEET_ID);
  // Check that we got a spreadsheet
  if(!spreadsheet)
  {
    return ContentService.createTextOutput("Spreadsheet not found");
  }
  
  var sheetDay = spreadsheet.getSheetByName(sheet.day);
  // Check that we got a sheet
  if(!sheetDay)
  {
    return ContentService.createTextOutput("Sheet not found");
  }
  
  // See if there's already a row with the current date, otherwise create it
  var rowId = -1;
  
  // Start from row number A2 as 1 is used for the header
  var rowOffset = 2;
  var dates = sheetDay.getRange('A' + rowOffset + ':A').getValues();
  
  for(var i in dates)
  { 
    var date = new Date(dates[i]);
    var year = date.getFullYear();
    var month = date.getMonth() + 1; // because January = 0
    var day = date.getDate();
    
    // Zerofill for month
    if(month < 10)
    {
      month = "0" + month;
    }
    
    // Zerofill for day
    if(day < 10)
    {
      day = "0" + day;
    }
    
    var dateFormatted = year + "-" + month + "-" + day;
    
    if(data.date == dateFormatted)
    {
      rowId = parseInt(i) + rowOffset;
      break;
    }
  }
  
  // Add a new row if there's no row with the current date
  if(rowId == -1)
  {
    // Get the row after the last *non-empty* row
    // It is not required to explicitly add a new row when there are no more empty rows available, it's done automatically
    var lastRow = sheetDay.getLastRow();
    
    // Since getLastRow() returns the value of the last non-empty row the value has to be increased manually
    rowId = lastRow + 1;
    
    // Update the date
    sheetDay.getRange(rowId, column.date).setValue(data.date);
    
    // Update the power sum formula using the row before it
    var powerTotal = sheetDay.getRange(lastRow, column.power).getFormulaR1C1();
    sheetDay.getRange(rowId, column.power).setFormulaR1C1(powerTotal);
    
    // Update the cost formula using 7 rows before it (to get the same day)
    var cost = sheetDay.getRange(rowId - 7, column.cost).getFormulaR1C1();
    sheetDay.getRange(rowId, column.cost).setFormulaR1C1(cost);
    
    // Update the cell formatting using 7 rows before it
    var backgrounds = sheetDay.getRange(rowId - 7, column.data, 1, 24).getBackgrounds();
    sheetDay.getRange(rowId, column.data, 1, 24).setBackgrounds(backgrounds);
  }
  
  // Get cell data
  var timeOffset = parseInt(d.getHours());
  var cellValue = sheetDay.getRange(rowId, column.data + timeOffset).getValue();
  
  // Only update the cell value if the cell is empty
  if(cellValue == "")
  {
    // Insert the data
    sheetDay.getRange(rowId, column.data + timeOffset).setValue(parseInt(data.power));
  }
  else
  {
    return ContentService.createTextOutput("Cell not empty");
  }
  
  // Update the month sheet
  var sheetMonth = spreadsheet.getSheetByName(sheet.month);
  
  // See if there's already a row with the current year/month, otherwise create it
  var rowId = -1;
  
  // Start from row number A2 as 1 is used for the header
  var rowOffset = 2;
  var months = sheetMonth.getRange('A' + rowOffset + ':A').getValues();
  
  for(var i in months)
  { 
    var date = new Date(months[i]);
    var year = date.getFullYear();
    var month = date.getMonth() + 1; // because January = 0
    
    if(d_y == year && d_m == month)
    {
      rowId = parseInt(i) + rowOffset;
      break;
    }
  }
  
  // Add a new row if there's no row with the current month, otherwise it's automatically updated with formulas
  if(rowId == -1)
  {
    // Get the row after the last *non-empty* row
    // It is not required to explicitly add a new row when there are no more empty rows available, it's done automatically
    var lastRow = sheetMonth.getLastRow();
    
    // Since getLastRow() returns the value of the last non-empty row the value has to be increased manually
    rowId = lastRow + 1;
    
    // Update the date
    sheetMonth.getRange(rowId, column.date).setValue(d_y + "-" + (d_m < 10 ? "0" + d_m : d_m) + "-01");
    
    // Update the power sum formula using the row before it
    var powerTotal = sheetMonth.getRange(lastRow, column.power).getFormulaR1C1();
    sheetMonth.getRange(rowId, column.power).setFormulaR1C1(powerTotal);
    
    // Update the cost formula using the row before it
    var cost = sheetMonth.getRange(lastRow, column.cost).getFormulaR1C1();
    sheetMonth.getRange(rowId, column.cost).setFormulaR1C1(cost);
    
    // Update all the formulas in the hours section
    var hours = sheetMonth.getRange(lastRow, column.data, 1, 24).getFormulasR1C1();
    sheetMonth.getRange(rowId, column.data, 1, 24).setFormulasR1C1(hours);
  }
  
  Logger.log("Entry successful");
  
  return ContentService.createTextOutput("OK");
}
