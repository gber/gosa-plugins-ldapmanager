<?php

class ldiftab extends tabs
{

  function __construct($config, $data, $dn)
  {
    tabs::__construct($config, $data, $dn);
  }

  function save_object($save_current= FALSE)
  {
    tabs::save_object($save_current);
  }


  function save($ignore_account= FALSE)
  {
  }

}


?>
