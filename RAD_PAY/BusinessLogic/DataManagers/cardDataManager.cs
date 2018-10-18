using RAD_PAY.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace RAD_PAY.BusinessLogic.ViewModels
{
    public class cardDataManager
    {
        //cardViewModel
        //public long card_id { get; set; }
        //public string number { get; set; }
        //public string expire { get; set; }
        //public long? uid { get; set; }
        //public int? is_primary { get; set; }
        //public string name { get; set; }
        //public int? tpl { get; set; }
        //public long? tr_limit { get; set; }
        //public int? block { get; set; }
        //public int? user_block { get; set; }
        //public string pc_token { get; set; }
        //public string owner { get; set; }
        //public int? foreign_card { get; set; }
        //public string owner_phone { get; set; }
        //public long daily_limit { get; set; }

        public static void Add(cardViewModel model, RAD_PAYEntities db)
        {

        }

        public static void Modify(cardViewModel model, RAD_PAYEntities db)
        {

        }

        public static void Delete(cardViewModel model, RAD_PAYEntities db)
        {

        }

        public static List<cardViewModel> Get(cardViewModel model, RAD_PAYEntities db)
        {
            return new List<cardViewModel>();
        }
    }
}