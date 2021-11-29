// https://stackoverflow.com/questions/22577457/update-data-on-a-page-without-refreshing
window.addEventListener('load', function()
{
    var xhr_files = null;
    var xhr_title = null;
    var xhr_devicename = null;

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

        xhr_files = new XMLHttpRequest();
        xhr_files.onreadystatechange = evenHandler_filelist;
        xhr_files.open("GET", "filelist", true);
        xhr_files.send(null);
    };

    updateLiveData_Once();

    function evenHandler_filelist()
    {
        if(xhr_files.readyState == 4 && xhr_files.status == 200)
        {
            dataDiv = document.getElementById('filelist');
            dataDiv.innerHTML = xhr_files.responseText;

        }
    }
    function evenHandler_title()
    {
        if(xhr_title.readyState == 4 && xhr_title.status == 200)
        {
            document.title = xhr_title.responseText;
            document.getElementById('title').innerHTML = xhr_title.responseText;
        }
    }
    function evenHandler_devicename()
    {
        if(xhr_devicename.readyState == 4 && xhr_devicename.status == 200)
        {
            document.getElementById('devicename').innerHTML = xhr_devicename.responseText;
        }
    }
});