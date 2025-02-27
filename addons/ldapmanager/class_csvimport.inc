<?php

class csvimport extends plugin
{
  /* Definitions */
  var $plHeadline= "LDIF export";
  var $plDescription= "This does something";
  var $access= "";

  /* Array with csv informations*/
  var $csvinfo=array();

  /* attribute list for save action */
  var $attributes= array();
  var $objectclasses= array();
  var $view_logged = FALSE;

  function __construct(&$config, $dn= NULL)
  {
    $this->initTime = microtime(TRUE);
    /* Include config object */
    $this->config= &$config;

    $this->ui = get_userinfo();
    stats::log('plugin', $class = get_class($this), $category = array($this->acl_category),  $action = 'open',
            $amount = 1, $duration = (microtime(TRUE) - $this->initTime));

  }

  function execute()
  {
    /* Call parent execute */
    plugin::execute();

    /* Log view */
    if(!$this->view_logged){
      $this->view_logged = TRUE;
      new log("view","all/".get_class($this),$this->dn);
    }

    /* initiate smarty */
    $smarty= get_smarty();
    $smarty->assign("selectedattrs",array(0=>""));
    $smarty->assign("data",array(0=>""));
    $smarty->assign("head",array(0=>""));
    $smarty->assign("sorted",0);
    $smarty->assign("fileup",0);

    /* Get the LDAP link, to generate the Export */
    $ldap = $this->config->get_ldap_link();

    $arrtemplates = array();
    $tempvar =0;

    /* Array to fill in Formfields */
    $arrtemplates[$tempvar] = "None";
    if(!is_array($this->csvinfo)){
      $this->csvinfo=array();
    }

    /* Set Usertemplate information and get all Attribute from userclass */
    unset ($this->csvinfo['arrtemplates']);
    unset ($this->csvinfo['arrtempls']);

    /* Generate Template Array, Attribute Array */
    if(!isset($this->csvinfo['arrtempls'])){

      /* Generating attributes */
      $attrs = @get_class_vars("user");
      $this->csvinfo['attr']  = $attrs['attributes'];

      /* Attach the choise none to discard a csv col */
      $new = array();
      $new[0]="none";
      $new[1]="userPassword";
      for($c = 0; $c < count($this->csvinfo['attr']);$c++){
        $new[$c+2]=$this->csvinfo['attr'][$c];
      }

      $this->csvinfo['attr'] = $new;

      /* Search all Templates    */
      $this->csvinfo['templates'] = $ldap->search("(objectClass=gosaUserTemplate)",array("*"));

      /* Array to handle selected Form fiels */
      $this->csvinfo['arrtempls'][$tempvar]['name'] = "";
      $this->csvinfo['arrtempls'][$tempvar]['dn']   = "";

      $this->csvinfo['arrtemplates'][]="None";

      while($temp = $ldap->fetch($this->csvinfo['templates'])){
        $tempvar ++;
        $this->csvinfo['arrtemplates'][$tempvar] = $temp['sn'][0]." - ".$this->config->idepartments[preg_replace("/^[^,]+,".preg_quote(get_people_ou(), '/')."/i", "", $temp['dn'])];
        $this->csvinfo['arrtempls']   [$tempvar]['name'] = $temp['sn'][0]; 
        $this->csvinfo['arrtempls']   [$tempvar]['dn']   = $temp['dn'];
      }
    }

    $arr_temp  = array_flip($this->csvinfo['attr']);
    $this->csvinfo['arr_selected']= array($arr_temp['uid'],$arr_temp['sn'],$arr_temp['givenName'],$arr_temp['userPassword']);

    $smarty->assign("templates",$this->csvinfo['arrtemplates']);
    $smarty->assign("attrs",$this->csvinfo['attr']);

    /* Check permissions for import */
    $acl = $this->ui->get_permissions($this->config->current['BASE'],"all/all");
    if(!preg_match("/w/",$acl)){
      if(isset($_POST['userfile']) || isset($_POST['sorted']) || isset($_POST['fileup'])){
      	msg_dialog::display(_("Permission error"), _("You've no permission to import CSV files!"), ERROR_DIALOG);
      }
      return ($smarty->fetch (get_template_path('contentcsv.tpl', TRUE)));
    }

    /* If the given dat from the csv File are sorted by the attributes */
    if(isset($_POST['sorted'])) {

      /* Template Handling */
      $smarty->assign("fileup",TRUE); 
      $smarty->assign("sorted",TRUE); 

      for($i = 0 ; $i < $this->csvinfo['rows'];$i++){
        /* Preparing given Data */
        $this->csvinfo['sorteddata'][$i]=array(); 

        /* Go through every col */
        for($a = 0; $a <= ($this->csvinfo['cols']); $a ++) {
          /* Append a status col at last, to show possibly errors */
          if($a==($this->csvinfo['cols'])){
            $this->csvinfo['sorteddata'][$i]["status"]="-";            
          } else {
            $cc = ($i * ($this->csvinfo['cols']))+$a;

            /* If theres a Attribut given for this col */
            if(!empty($_POST[('row'.($a))])) {
              $this->csvinfo['sorteddata'][$i][$this->csvinfo['attr'][$_POST[('row'.($a))]]]=$this->csvinfo['data'][($cc)]; 
            }
          }
        }
      } 

      /* aleast one entry */
      $rowcnt=0;      
      if(isset($this->csvinfo['sorteddata'])){
        $rowcnt ++;

        /* Set the col count */
        $tblcolcount= count ($this->csvinfo['cols']);

        /* entrie count */
        $tbl_entries= count ($this->csvinfo['count']);

        /* row count */
        $tbl_rows   = $tbl_entries / $tblcolcount;

        /* No error occurred */
        $failing = FALSE;

        /* For every row */
        for($i = 0; $i < $this->csvinfo['rows']; $i++){

          /* Get elemenet */
          $dat = $this->csvinfo['sorteddata'][$i];

          /* If not all needed vars set .... */
          if(         ( empty($dat['sn']))||( empty($dat['givenName']))||( empty($dat['uid']))||
              (!isset($dat['sn']))||(!isset($dat['givenName']))||(!isset($dat['uid']))){

            /* Output Error */
	    msg_dialog::display(_("Error"), sprintf(_("Need at least %s, %s and %s to create users!"), bold("sn"), bold("givenName"), bold("uid")), ERROR_DIALOG);
          } else {

            /* Register usertab to trigger edit dialog */
            $this->usertab= new usertabs($this->config, $this->config->data['TABS']['USERTABS'], 'new');
            $keys = array_keys($dat);


            /* Set all submitted vars  */
            if(is_array($keys)){
              foreach($keys as $key) {
                if($key != "status"){
                  $this->usertab->$key = $dat[$key];
                  foreach($this->usertab->by_object as $pname => $plugin){
                    if(isset($this->usertab->by_object[$pname]->$key)){
                      $this->usertab->by_object[$pname]->$key = $dat[$key];
                    }
                  }
                }
              }
            }

            /* Initialize template */
            if($this->csvinfo['tplid']!=0){
              $tpl_dn = $this->csvinfo['arrtempls'][$this->csvinfo['tplid']]['dn'];
              $this->usertab->adapt_from_template($tpl_dn,array("uid","cn","givenName","sn"));
              $tpl_base  = preg_replace("/^[^,]+,".preg_quote(get_people_ou(), '/i')."/", '', $tpl_dn);
              $this->usertab->by_object['user']->base= $tpl_base;
            } else {

              /* Reset vars */
              if(is_array($keys))
                foreach($keys as $key) {
                  if($key != "status"){
                    $this->usertab->by_object['user']->$key = $dat[$key];
                  }
                }
            }

            // Setting user Password 
            if((isset($dat['userPassword']))&&(!empty($dat['userPassword'])))
              $password = $dat['userPassword']; 
            else
            {
              $rand_str="";
              $feed = "0123456789abcdefghijklmnopqrstuvwxyz";
              for ($i=0; $i < 8; $i++)
              {
                $rand_str .= substr($feed, rand(0, strlen($feed)-1), 1);
              }
              $password = $rand_str;
            }

            /* Insert in the LDAP tree */
            if(count($this->usertab->check())) {
              msg_dialog::displayChecks($this->usertab->check());
              $this->csvinfo['sorteddata'][$i]['status']="<b>"._("failed")."</b>";
              $failing = $i+1;
              break;
            } else {
              $this->csvinfo['sorteddata'][$i]['status']=_("OK");
              $this->usertab->save();
              if(!change_password($this->usertab->dn,$password,FALSE,'','', $message)){
                msg_dialog::displayChecks(array($message));
              }
            }
          }
        }

        $pointsbefore = FALSE;
        $pointsafter  = FALSE;

        /* Get Attributs for Tableheadline  */
        $this->csvinfo['colsstr'] = array();
        for($i =0; $i <= $this->csvinfo['cols']; $i++) {
          if(!empty($_POST[('row'.$i)])){
            $this->csvinfo['colsstr'][$i] = $this->csvinfo['attr'][$_POST[('row'.$i)]];
          }
        }

        /*Create array with data for the template */
        if($failing == FALSE){

          $dataouts= array();
          for($i =1; $i <= $this->csvinfo['shownrowscount']; $i++) {
            if(is_array($this->csvinfo['sorteddata'][($i-1)])){
              foreach($this->csvinfo['sorteddata'][($i-1)] as $dat){
                $dataouts[]=$dat;
              }
            }
          }

          if($this->csvinfo['rows']> $this->csvinfo['shownrowscount']){
            $pointsafter = TRUE;
          }
        } else {
          $pointsbefore = TRUE;
          $pointsafter  = TRUE;

          $begin = $failing -3;

          if($begin <0) {
            $begin =0;
            $pointsbefore = FALSE;
          }

          $end = $failing + 2;

          if($end > $this->csvinfo['rows']) {
            $end = $this->csvinfo['rows']+1;
            $pointsafter = FALSE;
          }
          $this->csvinfo['shownrowscount']=$end - $begin;

          if($this->csvinfo['shownrowscount']> $this->csvinfo['rows'])$this->csvinfo['shownrowscount']=$this->csvinfo['rows'];

          $dataouts = array(); 
          for($i =$begin; $i <= $end; $i++) {
            if(is_array($this->csvinfo['sorteddata'][($i-1)])){
              foreach($this->csvinfo['sorteddata'][($i-1)] as $dat){
                $dataouts[]=$dat;
              }
            }
          }
        }
        $tmparr2  = $this->csvinfo['colsstr'];
        $tmparr2[]=_("status");

        /* Error? */
        if ($failing){
	  msg_dialog::display(_("Error"), sprintf(_("Cannot add entry %s!"), bold($failing)), ERROR_DIALOG);
        }
        $smarty->assign("error",$failing);

        /* Set vars to smarty */
        $smarty->assign("cols",count($tmparr2));
        $smarty->assign("anz" ,$this->csvinfo['count']);
        $smarty->assign("rows",$this->csvinfo['shownrowscount']);


        $cnt = 0 ;
        $tmp2 = $tmp3 = array();
        if(is_array($dataouts))
          foreach($dataouts as $tmp){
            $tmp2[] = $tmp;
            $cnt ++ ;
            if($cnt >= count($tmparr2)){
              $tmp3[] = $tmp2;
              $tmp2= array();
              $cnt = 0; 
            }
          }  
        $smarty->assign("head",$tmparr2);
        $smarty->assign("data",$tmp3);

        /* Set other vars  */
        $smarty->assign("i",1);
        $smarty->assign("ie",0);
        $smarty->assign("tplid",$this->csvinfo['tplid']);

        $smarty->assign("pointsafter",$pointsafter);
        $smarty->assign("pointsbefore",$pointsbefore);
      } else {
        /* Set Template ID */
        $tplid = $_POST['tplid'];


        $smarty->assign("tpl",$arrtemplates[$tplid]);
	msg_dialog::display(_("Information"), _("Nothing to import!"), INFO_DIALOG);
      }

      /* If theres a File uploaded */
    } else {
      /* Check if theres a file uploaded */
      if(!empty($_FILES['userfile']['name'])){

        $handle = NULL;

        $filename = gosa_file_name($_FILES['userfile']['tmp_name']);

        if((!isset($_FILES['userfile']['name']))||(!isset($_POST['fileup'])))
        {
            msg_dialog::display(_("Error"), sprintf(_("Cannot read uploaded file: %s"), _("file not found")), ERROR_DIALOG);
            $smarty->assign("LDIFError",TRUE);
        }
        elseif(!$_FILES['userfile']['size'] > 0 )
        {
            msg_dialog::display(_("Error"), sprintf(_("Cannot read uploaded file: %s"), _("file is empty")), ERROR_DIALOG);
            $smarty->assign("LDIFError",TRUE);
        }
        /* Is there a tmp file, which we can use ? */
        elseif(!file_exists($filename))
        {
            msg_dialog::display(_("Error"), sprintf(_("Cannot read uploaded file: %s"), _("file not found")), ERROR_DIALOG);
            $smarty->assign("LDIFError",TRUE);
        }
        elseif(!$handle = @fopen($filename,"r"))
        {
            msg_dialog::display(_("Error"), sprintf(_("Cannot read uploaded file: %s"), _("file not readable")), ERROR_DIALOG);
            $smarty->assign("LDIFError",TRUE);
        }
        else
        {
          $smarty->assign("fileup",TRUE);
          $str = "";


          /* Reading content */
          while(!feof($handle)) {
            $str .= fread($handle,1024);
          }

          $lines = preg_split("/\n/",$str);
          $anz  = 0;
          $rest = 0;
          $data = array();

          /* check colum count */
          if(is_array($lines))
            foreach($lines as $line) {
              /* continue if theres a comment */
              if(substr(trim($line),0,1)=="#"){
                continue;
              }

              $line= str_replace ("\t","",$line);
              $cells  = explode(",",$line )  ;

              if(count($cells)> $anz ){
                $anz = count($cells);
              }
            }

          /* At least one entry */
          if($anz >1) {

            /* Generate array with outpu info  */
            if(is_array($lines))
              foreach($lines as $line) {
                $rest = 0;
                $cnt  = 0;

                /* dont use comments or empty lines */
                if((substr(trim($line),0,1)=="#")||(empty($line))){
                  continue;
                }

                /* replace \t to , */
                $line= str_replace ("\t"  ,"" ,$line);

                /* get all elements  */
                $cells  = explode(",",$line )  ;

                /* attach all elements to data array */
                if(is_array($cells))
                  foreach($cells as $cell) {
                    if(!empty($cell)) {
                      $cnt++; 
                      $data[]=trim($cell);
                    }
                  }

                /* cell count less than anz, attach some empty fields */
                if(($cnt != $anz)&&(!empty($cnt))) {
                  $rest = $anz - $cnt;
                  for($i = 0 ; $i < $rest ; $i ++){
                    $data[] = " ";
                  }
                }
              }    

            unset($this->csvinfo['sorteddata']);
            unset($this->csvinfo['colsstr']);
            unset($this->csvinfo['sorteddata']);

            $this->csvinfo['cols']        = $anz;
            $this->csvinfo['data']        = array();
            $this->csvinfo['data']        = $data;
            $this->csvinfo['count']       = count($this->csvinfo['data']);

            if($this->csvinfo['count']> (6* $this->csvinfo['cols'])) {
              /* only show 6 rows in ouptuttablei */
              $datouttemp =  array_chunk($this->csvinfo['data'],(6* $this->csvinfo['cols']));
              $this->csvinfo['dataout']=$datouttemp[0];
              $this->csvinfo['shownrowscount'] = 6;
            } else {
              $this->csvinfo['shownrowscount'] = (count($this->csvinfo['data']))/$this->csvinfo['cols'];
              $this->csvinfo['dataout']= $this->csvinfo['data'];  
            }

            $this->csvinfo['tplid']       = $_POST['template'];
            $this->csvinfo['templatestr'] = $this->csvinfo['arrtemplates' ][$this->csvinfo['tplid']];
            $this->csvinfo['count']       = count($this->csvinfo['data']);
            $this->csvinfo['rows']        = (count($this->csvinfo['data'])/$anz);

            $i =  0; 
            $tmp = $tmp2= array();
            if(is_array($this->csvinfo['dataout']))
              foreach($this->csvinfo['dataout'] as $dat){
                $tmp[]= $dat;
                $i++;
                if($i >=$this->csvinfo['cols']){
                  $i = 0;
                  $tmp2[] = $tmp;
                  $tmp = array();
                }

              }

            /* Set Templateid  */
            $smarty->assign("tplid",$this->csvinfo['tplid']);

            /* Set Template  */
            $smarty->assign("tpl",$this->csvinfo['templatestr']);

            /* Temp var 1 */
            $smarty->assign("ia",1); 

            /* Temp var 2 */
            $smarty->assign("i",0); 

            /* Num rows    */
            $smarty->assign("rows",$this->csvinfo['shownrowscount']);


            for($i  = 0 ; $i < $anz; $i ++)
              $this->csvinfo['arr_selected'][]="0";

            $smarty->assign("selectedattrs",$this->csvinfo['arr_selected']);

            /* Entrie count5 */
            $smarty->assign("anz",$this->csvinfo['cols']);                                                    

            /* Array with data */
            $smarty->assign("data",$tmp2);   

            @fclose($handle);
          } else {
            $smarty->assign("tpl",$this->csvinfo['attr'][$_POST['template']]);
            $smarty->assign("LDIFError",TRUE);
            $smarty->assign("fileup",FALSE);
	    msg_dialog::display(_("Error"), _("Cannot find CSV data in the selected file!"), ERROR_DIALOG);
          }
        }
      }
    }                                              

    /* Show main page */
    return ($smarty->fetch (get_template_path('contentcsv.tpl', TRUE)));
  }

}


?>
