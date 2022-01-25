// https://stackoverflow.com/questions/22577457/update-data-on-a-page-without-refreshing
window.addEventListener('load', function()
{
    var xhr_status = null;
    var xhr_title = null;
    var xhr_devicename = null;

    updateLiveData_Status = function()
    {
        var url = 'statusupdate.html';
        xhr_status = new XMLHttpRequest();
        xhr_status.onreadystatechange = evenHandler_status;
        xhr_status.open("GET", url, true);
        xhr_status.send(null);
    };

    updateLiveData_Once = function()
    {
        xhr_title = new XMLHttpRequest();
        xhr_title.open("GET", "title", true);
        xhr_title.onreadystatechange = evenHandler_title;
        xhr_title.send(null);

        xhr_devicename = new XMLHttpRequest();
        xhr_devicename.onreadystatechange = evenHandler_devicename;
        xhr_devicename.open("GET", "devicename", true);
        xhr_devicename.send(null);
    };

    updateLiveData_Once();

    function evenHandler_status()
    {
        if(xhr_status.readyState == 4 && xhr_status.status == 200)
        {
            dataDiv = document.getElementById('status');
            dataDiv.innerHTML = xhr_status.responseText;
            window.setTimeout(updateLiveData_Status, 500);
        }
    }
    function evenHandler_title()
    {
        if(xhr_title.readyState == 4 && xhr_title.status == 200)
        {
            document.title = xhr_title.responseText;
            document.getElementById('title2').innerHTML = xhr_title.responseText;
        }
    }
    function evenHandler_devicename()
    {
        if(xhr_devicename.readyState == 4 && xhr_devicename.status == 200)
        {
            document.getElementById('devicename').innerHTML = xhr_devicename.responseText;
            window.setTimeout(updateLiveData_Status, 1);
        }
    }
});