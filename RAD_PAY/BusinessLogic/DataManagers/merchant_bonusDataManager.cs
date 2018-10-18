using RAD_PAY.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace RAD_PAY.BusinessLogic.ViewModels
{
    public class merchant_bonusDataManager
    {
        //merchant_bonusViewModel
        //public int merchant_id { get; set; }
        //public long? min_amount { get; set; }
        //public int? percent { get; set; }
        //public string description { get; set; }
        //public DateTime? start_date { get; set; }
        //public DateTime? end_date { get; set; }
        //public long id { get; set; }
        //public long? bonus_amount { get; set; }
        //public int? status { get; set; }
        //public double? longitude { get; set; }
        //public double? latitude { get; set; }
        //public int group_id { get; set; }

        public static void Add(merchant_bonusViewModel model, RAD_PAYEntities db)
        {

        }

        public static void Modify(merchant_bonusViewModel model, RAD_PAYEntities db)
        {

        }

        public static void Delete(merchant_bonusViewModel model, RAD_PAYEntities db)
        {

        }

        public static List<merchant_bonusViewModel> Get(merchant_bonusViewModel model, RAD_PAYEntities db)
        {
            return new List<merchant_bonusViewModel>();
        }
    }
}