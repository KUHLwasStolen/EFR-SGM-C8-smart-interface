async function sendSettings() {
    const newSettings = {
        import: document.getElementById("1.8.0").value,
        importT1: document.getElementById("1.8.1").value,
        importT2: document.getElementById("1.8.2").value,
        export: document.getElementById("2.8.0").value,
        exportT1: document.getElementById("2.8.1").value,
        exportT2: document.getElementById("2.8.2").value,
        unit: document.getElementById("unit").value
    }

    const statusElement = document.getElementsByClassName("put-status")[0];

    if(newSettings.unit.trim() === "") {
        statusElement.innerHTML = "Error: 'Unit' cannot be empty!";
        statusElement.style.color = "red";
        return;
    }

    const response = await fetch("", {
        method: "PUT",
        headers: {
            "Content-Type": "application/json"
        },
        body: JSON.stringify(newSettings)
    })

    if(response.ok) {
        statusElement.innerHTML = "Settings were successfully set!";
        statusElement.style.color = "green";
    } else {
        statusElement.innerHTML = "Something went wrong while trying to send settings!";
        statusElement.style.color = "red";
    }
}