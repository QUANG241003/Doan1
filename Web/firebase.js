// For Firebase JS SDK v7.20.0 and later, measurementId is optional
const firebaseConfig = {
    apiKey: "AIzaSyCE6qI-TG65bYmBhj7gNTKbby1EKKKPH6U",
    authDomain: "temperature-12-9.firebaseapp.com",
    databaseURL: "https://temperature-12-9-default-rtdb.firebaseio.com",
    projectId: "temperature-12-9",
    storageBucket: "temperature-12-9.appspot.com",
    messagingSenderId: "1036130638941",
    appId: "1:1036130638941:web:6510f64e9def89fe2d41b4",
    measurementId: "G-MSE8SLN2QG"
};

// Initialize Firebase
firebase.initializeApp(firebaseConfig);
firebase.analytics();

document.addEventListener('DOMContentLoaded', function() {
    const brightnessSlider = document.getElementById('brightness');
    const brightnessValue = document.getElementById('brightness-value');
    const ledImage = document.getElementById('led');
    const fanSpeedInput = document.getElementById('fan-speed');
    const btn1 = document.querySelector('#turn-on-button');
    const btnReset = document.querySelector('#reset-button'); // Thêm nút Reset

    // Lắng nghe sự thay đổi trong dữ liệu nhiệt độ
    const temperatureRef = firebase.database().ref('/temperature');
    temperatureRef.on('value', function(snapshot) {
        const temperature = snapshot.val();
        const temperatureElement = document.getElementById('temperature');
        temperatureElement.textContent = temperature;
    });

    // Lắng nghe sự thay đổi trong dữ liệu độ ẩm
    const humidityRef = firebase.database().ref('/doam');
    humidityRef.on('value', function(snapshot) {
        const humidity = snapshot.val();
        const humidityElement = document.getElementById('humidity');
        humidityElement.textContent = humidity;
    });

    brightnessSlider.addEventListener('input', function() {
        // Lấy giá trị hiện tại của thanh trượt
        let brightnessLevel = brightnessSlider.value;
        brightnessValue.textContent = brightnessLevel;
        ledImage.style.opacity = brightnessLevel / 100;
    });

    // Add event listener to the "Turn On" button
    btn1.addEventListener('click', function() {
        firebase.database().ref('permit').set(1);
        firebase.database().ref('Time').set(fanSpeedInput.value);
        firebase.database().ref('Setpoint').set(brightnessSlider.value);
    });

    // Add event listener to the "Reset" button
    btnReset.addEventListener('click', function() {
        firebase.database().ref('Time').set(0); // Đặt lại Time về 0
        firebase.database().ref('Setpoint').set(0);
        firebase.database().ref('permit').set(0); // Đặt lại Setpoint về 0
        // Đặt lại dienthoai về 0
    });
});