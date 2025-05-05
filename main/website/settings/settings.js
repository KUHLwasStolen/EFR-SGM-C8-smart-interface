async function sendSettings() {
    const newSettings = {
        import: parseFloat(document.getElementById("1.8.0").value),
        importT1: parseFloat(document.getElementById("1.8.1").value),
        importT2: parseFloat(document.getElementById("1.8.2").value),
        export: parseFloat(document.getElementById("2.8.0").value),
        exportT1: parseFloat(document.getElementById("2.8.1").value),
        exportT2: parseFloat(document.getElementById("2.8.2").value),
        unit: document.getElementById("unit").value
    }

    const statusElement = document.getElementsByClassName("put-status")[0];

    if(newSettings.unit.trim() === "") {
        statusElement.innerHTML = "Error: 'Currency' cannot be empty!";
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
        statusElement.innerHTML = "Success!";
        statusElement.style.color = "green";
    } else {
        statusElement.innerHTML = "Error: Failed to set settings!";
        statusElement.style.color = "red";
    }
}

document.getElementById("submit-button").addEventListener("click", () => sendSettings());