function sw(p){
    var a=p||'';
    x=new XMLHttpRequest();
    x.open('GET','switch?'+a,true);
    x.send();
    // ft=setTimeout(la,20000);
}
