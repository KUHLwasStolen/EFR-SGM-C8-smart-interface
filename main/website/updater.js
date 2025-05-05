let importPrice = 0.0, importPriceT1 = 0.0, importPriceT2 = 0.0, exportPrice = 0.0, exportPriceT1 = 0.0, exportPriceT2 = 0.0;

function reqListener() {
	const jsonObj = JSON.parse(this.responseText);

	document.getElementById("power-value").innerHTML = jsonObj.power.total;
	document.getElementById("power-l1-value").innerHTML = jsonObj.power.l1;
	document.getElementById("power-l2-value").innerHTML = jsonObj.power.l2;
	document.getElementById("power-l3-value").innerHTML = jsonObj.power.l3;
	document.getElementById("meter-import").innerHTML = jsonObj.meter.import.total;
  document.getElementById("meter-import-t1").innerHTML = jsonObj.meter.import.t1;
  document.getElementById("meter-import-t2").innerHTML = jsonObj.meter.import.t2;
	document.getElementById("meter-export").innerHTML = jsonObj.meter.export.total;
  document.getElementById("meter-export-t1").innerHTML = jsonObj.meter.export.t1;
  document.getElementById("meter-export-t2").innerHTML = jsonObj.meter.export.t2;
  document.getElementById("uptime-value").innerHTML = jsonObj.up;
  document.getElementById("uptime-value").style.color = "green";

  document.getElementById("import-cost-val0").innerHTML = (importPrice * parseFloat(jsonObj.meter.import.total)).toFixed(2);
  document.getElementById("import-cost-val1").innerHTML = (importPriceT1 * parseFloat(jsonObj.meter.import.t1)).toFixed(2);
  document.getElementById("import-cost-val2").innerHTML = (importPriceT2 * parseFloat(jsonObj.meter.import.t2)).toFixed(2);
  document.getElementById("export-cost-val0").innerHTML = (exportPrice * parseFloat(jsonObj.meter.export.total)).toFixed(2);
  document.getElementById("export-cost-val1").innerHTML = (exportPriceT1 * parseFloat(jsonObj.meter.export.t1)).toFixed(2);
  document.getElementById("export-cost-val2").innerHTML = (exportPriceT2 * parseFloat(jsonObj.meter.export.t2)).toFixed(2);

	let currentPower = parseInt(jsonObj.power.total);
	if (currentPower < 0) {
		document.getElementById("power-text").style.color = "green";
		document.getElementById("power-status").innerHTML = "export";
	} else if (currentPower > 0) {
		document.getElementById("power-text").style.color = "red";
		document.getElementById("power-status").innerHTML = "import";
	} else {
		document.getElementById("power-text").style.color = "black";
		document.getElementById("power-status").innerHTML = "idle";
	}
}

function errorListener() {
  document.getElementById("uptime-value").innerHTML = "Not reachable";
  document.getElementById("uptime-value").style.color = "red";
}

function updateAll() {
	const req = new XMLHttpRequest();
  req.timeout = 3500;
	req.addEventListener("load", reqListener);
  req.addEventListener("error", errorListener);
  req.addEventListener("timeout", errorListener);
	req.open("GET", "api/values");
	req.send();
}

function settingsReqListener() {
  const jsonObj = JSON.parse(this.responseText);

  const currencyElems = document.getElementsByClassName("currency");
  for(let i = 0; i < currencyElems.length; i++) {
    currencyElems[i].innerHTML = jsonObj.costs.currency;
  }

  importPrice = parseFloat(jsonObj.costs.import.overall);
  importPriceT1 = parseFloat(jsonObj.costs.import.T1);
  importPriceT2 = parseFloat(jsonObj.costs.import.T2);
  exportPrice = parseFloat(jsonObj.costs.export.overall);
  exportPriceT1 = parseFloat(jsonObj.costs.export.T1);
  exportPriceT2 = parseFloat(jsonObj.costs.export.T2);

  if(importPrice < 0.001) document.getElementById("import-cost").style.color = "lightgray";
  if(importPriceT1 < 0.001) document.getElementById("import-cost-t1").style.color = "lightgray";
  if(importPriceT2 < 0.001) document.getElementById("import-cost-t2").style.color = "lightgray";
  if(exportPrice < 0.001) document.getElementById("export-cost").style.color = "lightgray";
  if(exportPriceT1 < 0.001) document.getElementById("export-cost-t1").style.color = "lightgray";
  if(exportPriceT2 < 0.001) document.getElementById("export-cost-t2").style.color = "lightgray";
}

function getSettings() {
  const req = new XMLHttpRequest();
  req.addEventListener("load", settingsReqListener);
  req.open("GET", "api/settings");
  req.send();
}

getSettings();
updateAll();
setInterval(updateAll, 4000);
