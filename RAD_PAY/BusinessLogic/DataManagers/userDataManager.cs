﻿using RAD_PAY.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace RAD_PAY.BusinessLogic.ViewModels
{
    public class userDataManager
    {
        //userViewModel
        //public long id { get; set; }
        //public string phone { get; set; }
        //public string password { get; set; }
        //public string name { get; set; }
        //public DateTime registration { get; set; }
        //public int sex { get; set; }
        //public string notify_token { get; set; }
        //public string qrstring { get; set; }
        //public int? tr_limit { get; set; }
        //public string avatar { get; set; }
        //public string email { get; set; }
        //public string token { get; set; }
        //public int? lang { get; set; }
        //public int blocked { get; set; }
        //public DateTime? purch_ts { get; set; }
        //public bool sms_allow { get; set; }

        public static void Add(userViewModel model, RAD_PAYEntities db)
        {

        }

        public static void Modify(userViewModel model, RAD_PAYEntities db)
        {

        }

        public static void Delete(userViewModel model, RAD_PAYEntities db)
        {

        }

        public static List<userViewModel> Get(userViewModel model, RAD_PAYEntities db)
        {
            return new List<userViewModel>();
        }
    }
}