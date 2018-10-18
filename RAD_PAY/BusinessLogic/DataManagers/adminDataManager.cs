using RAD_PAY.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace RAD_PAY.BusinessLogic.ViewModels
{
    public class adminDataManager
    {
        //adminViewModel
        //public long   id          { get; set; }
        //public string login       { get; set; }
        //public string password    { get; set; }
        //public string first_name  { get; set; }
        //public int?   status        { get; set; }
        //public string last_name   { get; set; }
        //public int    flag           { get; set; }
        //public string phone       { get; set; }

        public static void Add(adminViewModel model, RAD_PAYEntities db)
        {
            var dbmodel = new admin
            {
                id          = model.id        ,
                login       = model.login     ,
                password    = model.password  ,
                first_name  = model.first_name,
                status      = model.status    ,
                last_name   = model.last_name ,
                flag        = model.flag      ,
                phone       = model.phone     ,
            };

            db.admins.Add(dbmodel);
        }

        public static void Modify(adminViewModel model, RAD_PAYEntities db)
        {

        }

        public static void Delete(adminViewModel model, RAD_PAYEntities db)
        {

        }

        public static List<adminViewModel> Get(adminViewModel model, RAD_PAYEntities db)
        {
            return new List<adminViewModel>();
        }
    }
}